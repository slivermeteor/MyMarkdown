#pragma once

#include <iostream>
#include "Markdown.h"
#include "Token.h"

namespace Markdown
{
	class CFileOperate
	{
	public:
		CFileOperate(size_t SpacesPerTab = DefaultSpacesPerTab);
		~CFileOperate();

		bool ReadFile(std::istream& ReadStream);
		bool WriteFile(std::ostream& WriteStream);

	private:
		// 私有变量
		bool m_IsProcessing;
		static const size_t DefaultSpacesPerTab;
		CTokenPtr m_TokenContainer;

		// 私有函数
		bool _GetLine(std::istream& ReadStream, std::string& Line);
		bool _Process();
		// HTML处理
		void _MergeMultilineHtmlTags();	// 将多行html标签合为一行
		// Markdown处理
		// 1. 区块元素处理
		void _ProcessBlockItems(CTokenPtr TokenContainer);
		// 2. 对于多段元素进行分段
		void _ProcessParagraphLines(CTokenPtr TokenContainer);
		void FlushParagraph(std::string& ParagraphText, CTokenList& ParagraphTokens, CTokenList& ProcessedList, bool bAsParagraph);
	};
}


