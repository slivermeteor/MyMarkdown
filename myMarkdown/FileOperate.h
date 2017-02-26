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
		// ˽�б���
		bool m_IsProcessing;
		static const size_t DefaultSpacesPerTab;
		CTokenPtr m_TokenContainer;

		// ˽�к���
		bool _GetLine(std::istream& ReadStream, std::string& Line);
		bool _Process();
		// HTML����
		void _MergeMultilineHtmlTags();	// ������html��ǩ��Ϊһ��
		// Markdown����
		// 1. ����Ԫ�ش���
		void _ProcessBlockItems(CTokenPtr TokenContainer);
		// 2. ���ڶ��Ԫ�ؽ��зֶ�
		void _ProcessParagraphLines(CTokenPtr TokenContainer);
		void FlushParagraph(std::string& ParagraphText, CTokenList& ParagraphTokens, CTokenList& ProcessedList, bool bAsParagraph);
	};
}


