#include "Markdown.h"
// 解释器全局函数

bool IsBlankLine(const std::string& Line)
{
	try
	{
		static const std::regex Expression(" {0,3}(<--(.*)-- *> *)* *");
		return std::regex_match(Line, Expression);
	}
	catch (std::regex_error e)
	{
		std::cout << e.what() << std::endl;
	}
	return false;
}

size_t CountQuoteLevel(const std::string& QuoteString)
{
	if (QuoteString.empty())
	{
		return 0;
	}
	size_t Level = 0;
	for (std::string::const_iterator i = QuoteString.begin(); i != QuoteString.end(); i++)
	{
		if (*i == '>')
		{
			Level++;
		}
	}

	return Level;
}


