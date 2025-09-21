#include "Scenario.h"
#include "String.h"
#define ScriptOffset 0x13F920

class ScriptParser
{
public:
	ScriptParser(const std::string& filename) : m_filename(filename) {};
	bool Parser(MalieExec& script);
	void ExportTotxt(MalieExec& script);
	void ExportOthers(MalieExec& script);

private:
	std::string m_filename;
	std::vector<char> m_buffer;
	size_t m_cursor = 0x170A;    //Skip Some redundant data
	size_t m_size = 0;
	void ParseFunctions(MalieExec& script);
	void ParseLabels(MalieExec& script);
	void ParseExpressions(MalieExec& script);
	std::unique_ptr<ExpressTree> ParseSingleTree();
	void ReadBytecode(MalieExec& script);
	std::wstring  ParseStringSegement(std::vector<unsigned char> TextBuff);
	TextSegment ParseSplit();
	void ParseMsgSegement(MalieExec& script);
	unsigned char ReadByte() { return m_buffer[m_cursor++]; }
	unsigned char PeekByte(size_t offset)
	{
		return (m_cursor + offset < m_size) ? (unsigned char)m_buffer[m_cursor + offset] : 0;
	}
};

bool ScriptParser::Parser(MalieExec& script)
{
	std::ifstream file(m_filename, std::ios::binary);
	if (!file)
	{
		std::cout << "Drag exec.dat to this program" << std::endl;
		return false;
	}
	m_buffer = std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	m_size = m_buffer.size();
	file.close();

	ParseFunctions(script);
	ParseLabels(script);

	m_cursor = 0x13F920;  //Script Offset
	auto scriptsize = ReadInt(m_cursor, m_buffer);

	while (m_cursor < (ScriptOffset + scriptsize))
	{
		script.StringSegement.push_back(ParseSplit());

		if (PeekByte(0) == 0x07 && PeekByte(1) == 0x06 && PeekByte(2) == 0x00)
		{
			m_cursor += 3; // skip 07 06 00
			std::cout << "Pos:" << m_cursor << std::endl;
		}
	}

	ParseMsgSegement(script);
	return true;
}

void ScriptParser::ParseFunctions(MalieExec& script)
{
	if (m_cursor > m_size) return;
	unsigned int count = ReadInt(m_cursor, m_buffer);
	for (int i = 0; i < count; i++)
	{
		auto len = ReadInt(m_cursor, m_buffer);
		auto text = ReadFileString(len, m_cursor, m_buffer);
		FunctionMan func(len, text);
		script.Function.push_back(func);
	}
}

void ScriptParser::ParseLabels(MalieExec& script)
{
	if (m_cursor > m_size) return;
	unsigned int count = ReadInt(m_cursor, m_buffer);
	for (int i = 0; i < count; i++)
	{
		auto len = ReadInt(m_cursor, m_buffer);
		auto text = ReadFileString(len, m_cursor, m_buffer);
		auto offset = ReadInt(m_cursor, m_buffer);
		LabelInfo label(len, text, offset);
		script.Label.push_back(label);
	}
}

void ScriptParser::ParseExpressions(MalieExec& script)
{
	if (m_cursor >= m_size) return;
	auto count = ReadInt(m_cursor, m_buffer);
	for (int i = 0; i < count; i++)
	{
		script.Express.push_back(ParseSingleTree());
	}
}

std::unique_ptr<ExpressTree> ScriptParser::ParseSingleTree()
{
	if (m_cursor >= m_size) return nullptr;

	unsigned char type = ReadByte();

	std::unique_ptr<ExpressTree> node;

	switch (type)
	{
	case 0x55:
		node = std::make_unique<ExpressTree>(ExpressTreeType::Node_NULL);
		node->data = std::monostate{};
		break;
	case 0x57:
		node = std::make_unique<ExpressTree>(ExpressTreeType::Node_INT);
		node->data = (int)ReadInt(m_cursor, m_buffer);
		break;
	case 0x56:
	case 0x58:
		node = std::make_unique<ExpressTree>(ExpressTreeType::Node_STRING);
		node->data = ReadFileString(ReadInt(m_cursor, m_buffer), m_cursor, m_buffer);
		break;
	default:
		node = std::make_unique<ExpressTree>(ExpressTreeType::Node_OPERATOR);
		OpNode children;
		children.left = ParseSingleTree();
		children.right = ParseSingleTree();
		node->data = std::move(children);
		break;
	}
	return node;
}

void ScriptParser::ReadBytecode(MalieExec& script)
{
	if (m_cursor >= m_size) return;
	auto size = ReadInt(m_cursor, m_buffer);
	if (m_cursor + size <= m_size)
	{
		script.ByteCode.assign(m_buffer.begin() + m_cursor, m_buffer.begin() + m_cursor + size);
		m_cursor + size;
	}
}

TextSegment ScriptParser::ParseSplit()
{
	TextSegment segement;

	if (PeekByte(0) == 0x07 && PeekByte(1) == 0x08)  //Voice
	{
		ReadByte();
		ReadByte();

		segement.Voice = ReadString0(m_buffer, m_cursor);
	}

	while (m_cursor < m_size)
	{
		if (PeekByte(0) == 0x07 && PeekByte(1) == 0x06 && PeekByte(2) == 0x00)  //Text End
		{
			break;
		}
		else if (PeekByte(0) == 0x07 && PeekByte(1) == 0x06)
		{
			std::cout << "* Not 0x070600 at Pos:" << m_cursor << std::endl;
		}

		segement.Text.push_back(ReadByte());
	}
	return segement;
}

std::wstring ScriptParser::ParseStringSegement(std::vector<unsigned char> TextBuff)
{
	std::wstringstream wss;
	std::string ss;
	size_t fs = 0;

	auto flushtowss = [&]() {
		if (!ss.empty()) {
			auto temp = ConvertToUtf16(ss);
			wss << temp;
		}
		ss.clear();
		};


	while (fs < TextBuff.size())
	{
		auto code = TextBuff[fs];
		bool isSjis = false;

		if ((code >= 0x81 && code <= 0x9F) || (code >= 0xE0 && code <= 0xFC)) {
			if (fs + 1 < TextBuff.size()) {
				unsigned char second_byte = TextBuff[fs + 1];
				if ((second_byte >= 0x40 && second_byte <= 0x7E) || (second_byte >= 0x80 && second_byte <= 0xFC)) {
					isSjis = true;
					ss.push_back(code);
					ss.push_back(second_byte);
					fs += 2;
					continue;
				}
			}
		}
		else if ((code >= 0x20 && code <= 0x7E) || (code >= 0xA1 && code <= 0xDF)) {
			isSjis = true;
			ss.push_back(code);
			fs++;
			continue;
		}


		if (!isSjis)
		{
			flushtowss();
			fs++;
			if (code == 0x07)
			{
				auto cmd = TextBuff[fs++];
				switch (cmd)
				{
				case 0x01:  //Ruby
				{
					std::stringstream ruby;
					while (fs < TextBuff.size() && TextBuff[fs] != 0x0A)
					{
						ruby << TextBuff[fs++];
					}
					fs++;
					wss << L"[Ruby]" << ConvertToUtf16(ruby.str()) << L"[/Ruby]";
					break;
				}
				case 0x04:
					wss << L"[0704]";
					break;
				case 0x06:
					wss << L"[End]";
					break;
				case 0x09:  //VoiceEnd
					wss << L"[/Voice]";
					break;
				default:
					wss << L"[hex:07" << std::hex << (int)cmd << L"]";
				}

			}
			else
			{
				wss << L"[hex:" << std::hex << (int)code << L"]";
			}
		}

	}
	flushtowss();
	return wss.str();
}

void ScriptParser::ParseMsgSegement(MalieExec& script)
{
	auto size = ReadInt(m_cursor, m_buffer);
	auto endpos = m_cursor + size;
	while (m_cursor < endpos)
	{
		auto str = ReadString0(m_buffer, m_cursor);
		script.MsgSegement.push_back(str);
	}
}

void ScriptParser::ExportTotxt(MalieExec& script)
{
	std::ofstream outfile("exec.txt", std::ios::binary);
	if (!outfile) return;

	unsigned char bom[2] = { 0xFF, 0xFE };
	outfile.write(reinterpret_cast<char*>(bom), 2);

	bool first = true;
	for (const auto& segement : script.StringSegement)
	{
		std::wstring line;
		if (!first)
		{
			line += L'\n';
			line += L'\n';
		}

		if (!segement.Voice.empty())
		{
			auto voice = ConvertToUtf16(segement.Voice.c_str());
			line += L"[Voice{" + voice + L"}]";
		}

		auto text = ParseStringSegement(segement.Text);
		line += text;
		outfile.write(reinterpret_cast<const char*>(line.data()), line.size() * sizeof(wchar_t));
		first = false;
	}
	outfile.close();


	std::ofstream outMsg("MsgStr.txt", std::ios::binary);
	if (!outMsg) return;

	outMsg.write(reinterpret_cast<char*>(bom), 2);

	for (const auto& msg : script.MsgSegement)
	{
		std::wstring line = ConvertToUtf16(msg);
		line += L'\n';
		outMsg.write(reinterpret_cast<const char*>(line.data()), line.size() * sizeof(wchar_t));
	}
	outMsg.close();
}

void ScriptParser::ExportOthers(MalieExec& script)
{
	std::ofstream outfile("labelstr.txt", std::ios::binary);
	if (!outfile) return;

	unsigned char bom[2] = { 0xFF, 0xFE };
	outfile.write(reinterpret_cast<char*>(bom), 2);

	for (const auto& fuc : script.Function)
	{
		std::wstring line = ConvertToUtf16(fuc.Name);
		line += L'\n';
		outfile.write(reinterpret_cast<const char*>(line.data()), line.size() * sizeof(wchar_t));
	}

	for (const auto& label : script.Label)
	{
		std::wstringstream wss;
		wss << L"Name:" << ConvertToUtf16(label.Name) << L" "
			<< L"Offset:" << std::hex << label.Offset << L'\n';
		std::wstring line = wss.str();
		outfile.write(reinterpret_cast<const char*>(line.data()), line.size() * sizeof(wchar_t));
	}
	outfile.close();
}


class ScriptBuild
{
public:
	ScriptBuild(const std::string& filename) : txtfilename(filename) {};
	bool Build(MalieExec& script);
	void ExportNewExec(MalieExec& script);
private:
	std::string txtfilename;
	std::vector<unsigned char> SerializeText(std::wstring& text);
	std::vector<unsigned char> SerializeMsg();
};


bool ScriptBuild::Build(MalieExec& script)
{
	std::ifstream infile(txtfilename, std::ios::binary);
	if (!infile)
	{
		std::cout << "Read ScriptTxt failed" << std::endl;
		return false;
	}
	infile.seekg(0, std::ios::end);
	size_t file_size = infile.tellg();
	infile.seekg(2, std::ios::beg);

	std::vector<char> byte_buffer(file_size - 2);
	infile.read(byte_buffer.data(), file_size - 2);
	infile.close();

	const wchar_t* ptr = reinterpret_cast<const wchar_t*>(byte_buffer.data());
	size_t len = byte_buffer.size() / sizeof(wchar_t);
	std::wstring segementText(ptr, len);

	size_t pos = 0;
	while (pos < segementText.length())
	{
		size_t next_separator = segementText.find(L"\n\n", pos);

		std::wstring segment_block;
		if (next_separator == std::wstring::npos)
		{
			segment_block = segementText.substr(pos);
			pos = segementText.length();
		}
		else
		{
			segment_block = segementText.substr(pos, next_separator - pos);
			pos = next_separator + 2;
		}

		if (segment_block.empty()) continue;
		TextSegment seg;

		if (segment_block.rfind(L"[Voice{", 0) == 0)
		{
			size_t voicestart = 7;  //[Voice{.length()
			size_t voiceend = segment_block.find(L"}]", voicestart);
			if (voiceend != std::wstring::npos)
			{
				std::wstring voice = segment_block.substr(voicestart, voiceend - voicestart);
				//seg.Voice = ConvertToSjis(voice);
				seg.Voice = ConvertToGBK(voice);
				size_t textend = segment_block.find(L"[/Voice]");
				auto text = segment_block.substr(voiceend + 2, textend - (voiceend + 2));
				seg.Text = SerializeText(text);
			}
		}
		else
		{
			seg.Text = SerializeText(segment_block);
		}
		script.StringSegement.push_back(seg);
		std::cout << "Pos:" << pos << std::endl;
	}
	return true;
}

std::vector<unsigned char> ScriptBuild::SerializeText(std::wstring& text)
{
	std::vector<unsigned char> buffer;
	size_t pos = 0;

	while (pos < text.length())
	{
		auto tagbegin = text.find(L"[", pos);
		if (tagbegin == std::wstring::npos) tagbegin = text.length();

		if (tagbegin > pos)
		{
			std::wstring textchunk = text.substr(pos, tagbegin - pos);
			//std::string chunk = ConvertToSjis(textchunk);
			std::string chunk = ConvertToGBK(textchunk);
			buffer.insert(buffer.end(), chunk.begin(), chunk.end());
		}

		pos = tagbegin;
		if (pos >= text.length()) break;

		if (text.compare(pos, 6, L"[Ruby]") == 0)
		{
			size_t tagend = text.find(L"[/Ruby]", pos);
			if (tagend != std::wstring::npos)
			{
				std::wstring ruby = text.substr(pos + 6, tagend - (pos + 6));

				buffer.push_back(0x07);
				buffer.push_back(0x01);
				auto rubyencode = ConvertToSjis(ruby);
				buffer.insert(buffer.end(), rubyencode.begin(), rubyencode.end());
				buffer.push_back(0x0A);
				pos = tagend + 7;
				continue;
			}
		}
		else if (text.compare(pos, 5, L"[End]") == 0)
		{
			buffer.push_back(0x07);
			buffer.push_back(0x06);
			pos += 5;
		}
		else if (text.compare(pos, 6, L"[0704]") == 0)
		{
			buffer.push_back(0x07);
			buffer.push_back(0x04);
			pos += 6;
		}
		else if (text.compare(pos, 5, L"[hex:") == 0)
		{
			size_t hexend = text.find(L"]", pos);
			if (hexend != std::wstring::npos)
			{
				std::wstring hex_str = text.substr(pos + 5, hexend - (pos + 5));
				int val = std::stoi(hex_str, nullptr, 16);
				buffer.push_back(static_cast<unsigned char>(val));
				pos = hexend + 1;
			}
		}
	}
	return buffer;
}

std::vector<unsigned char> ScriptBuild::SerializeMsg()
{
	std::vector<unsigned char> buffer;
	std::ifstream infile("MsgStr.txt", std::ios::binary);
	if (!infile)
	{
		std::cout << "Read MsgFile failed" << std::endl;
	}

	infile.seekg(2, std::ios::beg);

	auto ReadLine = [&]() -> std::wstring
		{
			std::wstring line;
			wchar_t ch;

			while (infile.read(reinterpret_cast<char*>(&ch), sizeof(wchar_t)))
			{
				if (ch == L'\n') break;
				line.push_back(ch);
			}

			return line;
		};
	auto PeekLine = [&]() -> std::wstring
		{
			auto pos = infile.tellg();
			std::wstring line;
			wchar_t ch;

			while (infile.read(reinterpret_cast<char*>(&ch), sizeof(wchar_t)))
			{
				if (ch == L'\n') break;
				line.push_back(ch);
			}
			infile.clear();
			infile.seekg(pos);
			return line;
		};

	while (true)
	{
		auto current = ReadLine();
		if (infile.eof() && current.empty()) break;
		auto next = PeekLine();
		if (!current.empty())
		{
			std::string str = ConvertToGBK(current);
			buffer.insert(buffer.end(), str.begin(), str.end());
		}
		if (next.empty() && !current.empty())
		{
			buffer.push_back(0x0A);
			buffer.push_back(0x00);
		}
		else if (!next.empty() && !current.empty())
		{
			buffer.push_back(0x00);
		}
		
	}
	return buffer;
}


void ScriptBuild::ExportNewExec(MalieExec& script)
{
	std::ifstream infile("exec.dat", std::ios::binary);
	if (!infile)
	{
		std::cout << "Original exec.dat connot find" << std::endl;
	}

	std::vector<char> oribuffer = std::vector<char>((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
	infile.close();

	std::vector<unsigned char> scriptblock;
	scriptblock.insert(scriptblock.end(), { 0x00, 0x00, 0x00, 0x00 });

	for (auto seg : script.StringSegement)
	{
		if (!seg.Voice.empty())
		{
			scriptblock.push_back(0x07);
			scriptblock.push_back(0x08);
			scriptblock.insert(scriptblock.end(), seg.Voice.begin(), seg.Voice.end());
			scriptblock.push_back(0x00);
			scriptblock.insert(scriptblock.end(), seg.Text.begin(), seg.Text.end());
			scriptblock.push_back(0x07);
			scriptblock.push_back(0x09);
			scriptblock.push_back(0x07);
			scriptblock.push_back(0x06);
			scriptblock.push_back(0x00);
		}
		else
		{
			scriptblock.insert(scriptblock.end(), seg.Text.begin(), seg.Text.end());
			scriptblock.push_back(0x07);
			scriptblock.push_back(0x06);
			scriptblock.push_back(0x00);
		}
	}

	auto size = scriptblock.size();
	*reinterpret_cast<unsigned int*>(scriptblock.data()) = size - 4;

	std::ofstream outfile("execNew.dat", std::ios::binary | std::ios::trunc);

	outfile.write(oribuffer.data(), ScriptOffset);
	outfile.write(reinterpret_cast<const char*>(scriptblock.data()), size);

	std::vector<unsigned char> msgblock;
	auto msgdata = SerializeMsg();
	uint32_t len = static_cast<uint32_t>(msgdata.size());
	msgblock.resize(4 + len);
	memcpy(msgblock.data(), &len, 4);
	memcpy(msgblock.data() + 4, msgdata.data(), msgdata.size());
	outfile.write(reinterpret_cast<const char*>(msgblock.data()), msgblock.size());

	outfile.close();
}


int main(int argc, char* argv[])
{
	MalieExec Script;

#if false	
	ScriptParser parser("exec.dat");
	parser.Parser(Script);
	parser.ExportTotxt(Script);
	//parser.ExportOthers(Script);
#else
	ScriptBuild builder("exec.txt");
	builder.Build(Script);  //Delete the last blank line of MsgStr.txt manually :(
	builder.ExportNewExec(Script);
#endif

}
