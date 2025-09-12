#include "Scenario.h"
#include "String.h"

std::string ReadString0(std::vector<char>& buffer, size_t& pt)
{
	std::string str;
	while (buffer[pt] != '\0')
	{
		str += buffer[pt++];
	}
	pt++;

	return str;
}

std::string ReadStringUntil(std::vector<char>& buffer, size_t& pt, unsigned char terminate)
{
	std::string str;
	while (buffer[pt] != terminate)
	{
		str += buffer[pt++];
	}
	pt++;

	return str;
}

std::wstring ConvertToUtf16(const std::string& str) 
{
	if (str.empty()) return L"";
	int wide_len = MultiByteToWideChar(932, 0, str.c_str(), str.length(), NULL, 0);
	std::wstring wide_buffer(wide_len, 0);
	MultiByteToWideChar(932, 0, str.c_str(), str.length(), &wide_buffer[0], wide_len);
	return wide_buffer;
}

std::string ConertToGBK(const std::wstring& str)
{
	if (str.empty()) return "";
	int gbk_len = WideCharToMultiByte(936, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
	std::string gbk_buffer(gbk_len, 0);
	WideCharToMultiByte(936, 0, str.c_str(), str.length(), &gbk_buffer[0], gbk_len, NULL, NULL);
	return gbk_buffer;
}

unsigned int ReadInt(size_t& fs, std::vector<char>& buffer)
{
	unsigned int val = 0;
	val = *reinterpret_cast<unsigned int*>(&buffer[fs]);
	fs += 4;
	return val;
}

std::string ReadFileString(int len, size_t& fs, std::vector<char>& buffer)
{
	std::string str;
	if (fs + len < buffer.size())
	{
		str.assign(&buffer[fs], len);
		fs += len;
	}
	return str;
}