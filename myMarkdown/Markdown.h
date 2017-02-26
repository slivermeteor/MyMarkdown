#pragma once
#include <iostream>
#include <memory> // shard_ptr 
#include <list>
#include <regex>

#define ULONG unsigned long 

// 命名空间 基本类型定义
namespace Markdown
{
	class CToken;
	typedef std::shared_ptr<CToken> CTokenPtr;
	typedef std::list<CTokenPtr> CTokenList;

	typedef CTokenList::const_iterator CTokenListCIter;
	typedef CTokenList::iterator CTokenListIter;

	

}

// 全局函数声明
bool IsBlankLine(const std::string& Line);

size_t CountQuoteLevel(const std::string& QuoteString);


