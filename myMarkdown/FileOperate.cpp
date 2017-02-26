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

		// CContainer��ĵ�һ��Ա ����һ�� CTokenList 
		// ����ǰ���CTokenPtr(�ڳ�ʼ����ʱ�򣬾���Ϊһ��CContainer)��Ϊһ��CTokenList��Ϊһ��List
		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(m_TokenContainer.get());
		std::string Line;
		CTokenList TokenList;
		while (_GetLine(ReadStream, Line))
		{
			if (IsBlankLine(Line))
			{
				// ���� - ���������
				TokenList.push_back(CTokenPtr(new Token::CBlankLine(Line)));
			}
			else
			{
				// ��ÿ�����ݷ���Token������ - �û����ָ��List���洢����
				TokenList.push_back(CTokenPtr(new Token::CRawText(Line)));
			} 
		}

		// �����ж�ȡ��ϣ��õ������������ݶ�Ӧ��Token��ָ��List
		// �����List ���� Containers ���List
		Tokens->AddSubTokens(TokenList);

		return true;
	}

	bool CFileOperate::WriteFile(std::ostream& WriteStream)
	{
		_Process();
		m_TokenContainer->WriteAsHTML(WriteStream);
		return true;
	}

	// ˽���ڲ�����ʵ��
	bool CFileOperate::_GetLine(std::istream & ReadStream, std::string & Line)
	{
		Line.clear();
		char c = 0;

		// ���� \r, \r\n, \n, \n\r
		while (ReadStream.get(c))
		{
			if (c == '\r')
			{
				// �ж���һ���ַ� �� \n �� - �ǣ����������һ�ж��룬���� - �񣬻���һ��
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
				// ���� Tab ��, ���뵱ǰ����
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
			// HTML ����
			_MergeMultilineHtmlTags();

			// ���� Markdown
			_ProcessBlockItems(m_TokenContainer);
			_ProcessParagraphLines(m_TokenContainer);
			m_TokenContainer->ProcessSectorElements();

			m_IsProcessing = true;			
		}

		return true;
	}

	// HTML ����
	void CFileOperate::_MergeMultilineHtmlTags()
	{
		// ����ƥ�� ��ǰ�� ��һ����������HTML��ǩ
		static const std::regex HtmlTokenStart("<((/?)([a-zA-Z0-9]+)(?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\5))*? */? *))$");
		static const std::regex HtmlTokenEnd("^ *((?:( +[a-zA-Z0-9]+?(?: ?= ?(\"|').*?\\3))*? */? *))>");

		CTokenList ProcessedList;

		Token::CContainer* Tokens = dynamic_cast<Token::CContainer*>(m_TokenContainer.get());

		for (CTokenList::const_iterator i = Tokens->SubTokens().begin(); i != Tokens->SubTokens().end(); i++)
		{
			if (!((*i)->Text().empty()) && std::regex_match((*i)->Text(), HtmlTokenStart))
			{
				CTokenList::const_iterator next = i++;
				// ��һ��ƥ�� HTML��ǩ����
				if (next != Tokens->SubTokens().end() && !(*next)->Text().empty() && std::regex_match((*next)->Text(), HtmlTokenEnd))
				{
					// ���кϲ�һ��
					ProcessedList.push_back(CTokenPtr(new Markdown::Token::CRawText((*i)->Text() + ' ' + (*next)->Text())));
				}
			}
			// ���Ƿָ��HTML�� ֱ�ӷ���ProcessedList
			ProcessedList.push_back(*i);
		}
		// ����Tokens
		Tokens->SwapSubTokens(ProcessedList);
	}

	// Markdown ����
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
					// �����ǰToken��һ��BlockElement ��ô���п�����Ƕ BlockElement����Ҫ�ݹ鴦��
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
	// ÿ��ĩβ ˫�ո� ���д��� -> <BR />
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
	// �кϲ�����
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


