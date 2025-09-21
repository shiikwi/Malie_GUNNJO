#pragma once
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <Windows.h>
#include <variant>

struct FunctionMan
{
	unsigned int Length;
	std::string Name;
	FunctionMan(int len, std::string name) : Length(len), Name(name) {}
};

struct LabelInfo
{
	unsigned int Length;
	std::string Name;
	int Offset;
	LabelInfo(int len, std::string name, int off) : Length(len), Name(name), Offset(off) {}
};

enum ExpressTreeType : int
{
	Node_NULL = 0x55,    //U
	Node_STRING = 0X56,  //V
	/*	Node_STRING = 0x58,*/  //X
	Node_INT = 0x57,     //W
	Node_OPERATOR = 0x00
};

struct ExpressTree;

struct OpNode
{
	std::unique_ptr<ExpressTree> left;
	std::unique_ptr<ExpressTree> right;
};

struct ExpressTree
{
	ExpressTreeType type;
	std::variant<int, std::string, OpNode, std::monostate> data;

	ExpressTree(ExpressTreeType t) : type(t) {};
};

struct TextSegment
{
	std::string Voice;
	std::vector<unsigned char> Text;
};

class MalieExec
{
public:
	std::vector<FunctionMan> Function;
	std::vector<LabelInfo> Label;
	std::vector<std::unique_ptr<ExpressTree>> Express;
	std::vector<unsigned char> ByteCode;
	std::vector<TextSegment> StringSegement;
	std::vector<std::string> MsgSegement;
};