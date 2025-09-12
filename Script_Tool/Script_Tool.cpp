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

	ParseFunctions(script);
	ParseLabels(script);

	m_cursor = 0x13F920;  //Script Offset
	auto scriptsize = ReadInt(m_cursor, m_buffer);

	if (m_cursor < (ScriptOffset + scriptsize)) {
		script.StringSegement.push_back(ParseSplit());
	}

	while (m_cursor < (ScriptOffset + scriptsize))
	{
		if (PeekByte(0) == 0x07 && PeekByte(1) == 0x06)
		{
			m_cursor += 3; // skip 07 06 00
			std::cout << "Pos:" << m_cursor << std::endl;
			if (m_cursor >= (ScriptOffset + scriptsize)) break;
			script.StringSegement.push_back(ParseSplit());
		}
		else
		{
			break;
		}
	}
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
		if (PeekByte(0) == 0x07 && PeekByte(1) == 0x06)  //Text End
		{
			break;
		}

		segement.Text.push_back(ReadByte());
	}
	return segement;
}

std::wstring ScriptParser::ParseStringSegement(std::vector<unsigned char> TextBuff)
{
	std::wstringstream wss;
	std::stringstream ss;
	size_t fs = 0;

	auto flushtowss = [&]() {
		if (!ss.str().empty()) {
			auto temp = ConvertToUtf16(ss.str());
			wss << temp;
		}
		ss.str("");
		ss.clear();
		};


	while (fs < TextBuff.size())
	{
		auto code = TextBuff[fs];
		fs++;
		bool isSjis = false;

		if ((code >= 0x81 && code <= 0x9F) || (code >= 0xE0 && code <= 0xFC)) {
			if (fs < TextBuff.size()) {
				unsigned char second_byte = TextBuff[fs];
				if ((second_byte >= 0x40 && second_byte <= 0x7E) || (second_byte >= 0x80 && second_byte <= 0xFC)) {
					isSjis = true;
					ss << code << TextBuff[fs++];
				}
			}
		}
		else if ((code >= 0x20 && code <= 0x7E) || (code >= 0xA1 && code <= 0xDF)) {
			isSjis = true;
			ss << code;
		}


		if (!isSjis)
		{
			flushtowss();
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

void ScriptParser::ExportTotxt(MalieExec& script)
{
	std::ofstream outfile("exec.txt", std::ios::binary);
	if (!outfile) return;

	unsigned char bom[2] = { 0xFF, 0xFE };
	outfile.write(reinterpret_cast<char*>(bom), 2);

	for (const auto& segement : script.StringSegement)
	{
		std::wstring line;
		if (!segement.Voice.empty())
		{
			auto voice = ConvertToUtf16(segement.Voice.c_str());
			line += L"[Voice{" + voice + L"}]";
		}

		auto text = ParseStringSegement(segement.Text);
		line += text;
		line += L'\n';
		line += L'\n';
		outfile.write(reinterpret_cast<const char*>(line.data()), line.size() * sizeof(wchar_t));
	}
}

void ScriptParser::ExportOthers(MalieExec& script)
{
	std::ofstream outfile("str.txt", std::ios::binary);
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
}


int main(int argc, char* argv[])
{
	MalieExec Script;

	ScriptParser parser("exec.dat");
	parser.Parser(Script);
	parser.ExportTotxt(Script);
	//parser.ExportOthers(Script);
}
