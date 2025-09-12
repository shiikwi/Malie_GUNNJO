#pragma once
#include <iostream>
#include <vector>
#include <Windows.h>

std::string ReadString0(std::vector<char>& buffer, size_t& pt);
std::string ReadStringUntil(std::vector<char>& buffer, size_t& pt, unsigned char terminate);
std::wstring ConvertToUtf16(const std::string& str);
std::string ConertToGBK(const std::wstring& str);
unsigned int ReadInt(size_t& fs, std::vector<char>& buffer);
std::string ReadFileString(int len, size_t& fs, std::vector<char>& buffer);