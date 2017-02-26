#include "FileOperate.h"


namespace Markdown
{
	const size_t CFileOperate::DefaultSpacesPerTab = 4;

	CFileOperate::CFileOperate(size_t SpacesPerTab):
		m_TokenContainer(new Token::CContainer())
	{
		m_IsProcessing = false;
	}


	CFileOperate::~CFileOperate()
	{
	}

	bool CFileOperate::ReadFile(std::istream& ReadStream)
	{
		if (m_IsProcessing)
		{
			return false;
		}

		// CContainer类的第一成员 就是一个 CTokenList 
		// 将当前类的CTokenPtr(在初始化的时候，就作为一个CContainer)作为一个CTokenList作为一个List
		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(m_TokenContainer.get());
		std::string Line;
		CTokenList TokenList;
		while (_GetLine(ReadStream, Line))
		{
			if (IsBlankLine(Line))
			{
				// 空行 - 放入空行类
				TokenList.push_back(CTokenPtr(new Token::CBlankLine(Line)));
			}
			else
			{
				// 将每行内容放入Token派生类 - 用基类的指针List来存储他们
				TokenList.push_back(CTokenPtr(new Token::CRawText(Line)));
			} 
		}

		// 所有行读取完毕，得到是所有行内容对应的Token类指针List
		// 将这个List 放入 Containers 里的List
		Tokens->AddSubTokens(TokenList);

		return true;
	}

	bool CFileOperate::WriteFile(std::ostream& WriteStream)
	{
		_Process();
		m_TokenContainer->WriteAsHTML(WriteStream);
		return true;
	}

	// 私有内部函数实现
	bool CFileOperate::_GetLine(std::istream & ReadStream, std::string & Line)
	{
		Line.clear();
		char c = 0;

		// 处理 \r, \r\n, \n, \n\r
		while (ReadStream.get(c))
		{
			if (c == '\r')
			{
				// 判断下一个字符 是 \n 吗？ - 是，就完成了这一行读入，结束 - 否，回退一格
				if (ReadStream.get(c) && c != '\n')
				{
					ReadStream.unget();
				}
				return true;
			}
			else if (c == '\n')
			{
				if (ReadStream.get(c) && c != '\r')
				{
					ReadStream.unget();
				}
				return true;
			}
			else if (c == '\t')
			{
				// 读到 Tab 符, 对齐当前长度
				Line += std::string(DefaultSpacesPerTab - (Line.length() % DefaultSpacesPerTab), ' ');
			}
			else
			{
				Line.push_back(c);
			}
		}

		return !Line.empty();
	}

	bool CFileOperate::_Process()
	{
		if (!m_IsProcessing)
		{
			// HTML 处理
			_MergeMultilineHtmlTags();

			// 处理 Markdown
			_ProcessBlockItems(m_TokenContainer);
			_ProcessParagraphLines(m_TokenContainer);
			m_TokenContainer->ProcessSectorElements();

			m_IsProcessing = true;			
		}

		return true;
	}

	// HTML 处理
	void CFileOperate::_MergeMultilineHtmlTags()
	{
		// 正则匹配 当前行 是一个不完整的HTML标签
		static const std::regex HtmlTokenStart("<((/?)([a-zA-Z0-9]+)(?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\5))*? */? *))$");
		static const std::regex HtmlTokenEnd("^ *((?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\3))*? */? *))>");

		CTokenList ProcessedList;

		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(m_TokenContainer.get());

		for (CTokenList::const_iterator i = Tokens->SubTokens().begin(); i != Tokens->SubTokens().end(); i++)
		{
			if (!((*i)->Text().empty()) && std::regex_match((*i)->Text(), HtmlTokenStart))
			{
				CTokenList::const_iterator next = i++;
				// 下一行匹配 HTML标签结束
				if (next != Tokens->SubTokens().end() && !(*next)->Text().empty() && std::regex_match((*next)->Text(), HtmlTokenEnd))
				{
					// 两行合并一行
					ProcessedList.push_back(CTokenPtr(new Markdown::Token::CRawText((*i)->Text() + ' ' + (*next)->Text())));
				}
			}
			// 不是分割的HTML行 直接放入ProcessedList
			ProcessedList.push_back(*i);
		}
		// 更新Tokens
		Tokens->SwapSubTokens(ProcessedList);
	}

	// Markdown 处理
	void CFileOperate::_ProcessBlockItems(CTokenPtr TokenContainer)
	{
		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(TokenContainer.get());
		if (Tokens == NULL)
		{
			return;
		}

		CTokenList ProcessedList;
		CTokenList::const_iterator e = Tokens->SubTokens().end();
		for (CTokenList::const_iterator i = Tokens->SubTokens().begin(); i != e; i++)
		{
			if (!(*i)->Text().empty())
			{
				CTokenPtr Token = nullptr;
				if (Token == nullptr)
				{
					Token = ParseHeader(i, e);
				}
				if (Token == nullptr)
				{
					Token = ParseHorizontalRule(i, e);
				}
				if (Token == nullptr)
				{
					Token = ParseListBlock(i, e);
				}
				if (Token == nullptr)
				{
					Token = ParseBlockQuote(i, e);
				}
				if (Token == nullptr)
				{
					Token = ParseCodeBlock(i, e);
				}
				
				if (Token != nullptr)
				{
					// 如果当前Token是一种BlockElement 那么它有可能内嵌 BlockElement所有要递归处理
					_ProcessBlockItems(Token);
					ProcessedList.push_back(Token);
					if (i == e)
					{
						break;
					}
					continue;
				}
				else
				{
					ProcessedList.push_back(*i);
				}
			}
			else if ((*i)->IsContainer())
			{
				_ProcessBlockItems(*i);
				ProcessedList.push_back(*i);
			}
		}

		Tokens->SwapSubTokens(ProcessedList);
	}
	// 每行末尾 双空格 换行处理 -> <BR />
	void CFileOperate::_ProcessParagraphLines(CTokenPtr TokenContainer)
	{
		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(TokenContainer.get());
		if (Tokens == nullptr)
		{
			return;
		}

		bool bAsParagraph = Tokens->InhibitParagraphs();
		for (CTokenList::const_iterator i = Tokens->SubTokens().begin(); i != Tokens->SubTokens().end(); i++)
		{
			if ((*i)->IsContainer())
			{
				_ProcessParagraphLines(*i);
			}
		}

		CTokenList ProcessedList;
		CTokenList ParagraphTokens;
		std::string ParagraphText;
		for (CTokenList::const_iterator i = Tokens->SubTokens().begin(); i != Tokens->SubTokens().end(); i++)
		{
			if (!(*i)->Text().empty() && (*i)->CanContainMarkup() && !(*i)->InhibitParagraphs())
			{
				static const std::regex ParagraphTextRegex("^(.*)  $");
				if (!ParagraphText.empty())
				{
					ParagraphText += " ";
				}

				std::smatch MatchResult;
				if (std::regex_match((*i)->Text(), MatchResult, ParagraphTextRegex))
				{
					ParagraphText += MatchResult[1];
					FlushParagraph(ParagraphText, ParagraphTokens, ProcessedList, bAsParagraph);
					ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("br/")));
				}
				else
				{
					ParagraphText += (*i)->Text();
				}
			}
			else
			{
				FlushParagraph(ParagraphText, ParagraphTokens, ProcessedList, bAsParagraph);
				ProcessedList.push_back(*i);
			}
		}
		FlushParagraph(ParagraphText, ParagraphTokens, ProcessedList, bAsParagraph);
		
		Tokens->SwapSubTokens(ProcessedList);
	}
	// 行合并函数
	void CFileOperate::FlushParagraph(std::string& ParagraphText, CTokenList& ParagraphTokens, CTokenList& ProcessedList, bool bAsParagraph)
	{
		if (!ParagraphText.empty())
		{
			ParagraphTokens.push_back(CTokenPtr(new Token::CRawText(ParagraphText)));
			ParagraphText.clear();
		}

		if (!ParagraphTokens.empty())
		{
			if (bAsParagraph)
			{
				if (ParagraphTokens.size() > 1)
				{
					ProcessedList.push_back(CTokenPtr(new Token::CContainer(ParagraphTokens)));
				}
				else
				{
					ProcessedList.push_back(*ParagraphTokens.begin());
				}
			}
			else
			{
				ProcessedList.push_back(CTokenPtr(new Token::CParagraph(ParagraphTokens)));
			}
			ParagraphTokens.clear();
		}
	}
}


