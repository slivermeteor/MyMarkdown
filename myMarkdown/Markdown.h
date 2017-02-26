#pragma once
#include <iostream>
#include <memory> // shard_ptr 
#include <list>
#include <regex>

#define ULONG unsigned long 

// �����ռ� �������Ͷ���
namespace Markdown
{
	class CToken;
	typedef std::shared_ptr<CToken> CTokenPtr;
	typedef std::list<CTokenPtr> CTokenList;

	typedef CTokenList::const_iterator CTokenListCIter;
	typedef CTokenList::iterator CTokenListIter;

	

}

// ȫ�ֺ�������
bool IsBlankLine(const std::string& Line);

size_t CountQuoteLevel(const std::string& QuoteString);


