#include "Token.h"

namespace Markdown
{
	// ���������ַ�
	std::string TransCharacter(std::string strSour, ULONG TranscodingFlags)
	{
		if (strSour.empty())
		{
			return std::string();
		}

		std::string ProcessedList;
		for (std::string::const_iterator i = strSour.begin(); i != strSour.end(); i++)
		{
			if (*i == '&' && (TranscodingFlags & SelectTransferAmps) != 0)
			{
				// ����������²�ת�� & - 1.&amp 2.16λ��λ��
				static const std::regex NoTransferRegex("^(&amp;)|(&#[xX][0-9a-fA-F]{1,2};)|(&#[0-9a-fA-F]{1,3};)");
				if (std::regex_search(i, i + 5, NoTransferRegex))
				{
					ProcessedList.push_back(*i);
				}
				else
				{
					ProcessedList += "&amp;";
				}
			}
			else if (*i == '&' && (TranscodingFlags & TransferAmps) != 0)
			{
				ProcessedList += "&amp;";
			}
			else if (*i == '<' && ((TranscodingFlags & TransferCompar) != 0))
			{
				ProcessedList += "&lt;";
			}
			else if (*i == '>' && (TranscodingFlags & TransferCompar) != 0)
			{
				ProcessedList += "&gt;";
			}
			else
			{
				ProcessedList.push_back(*i);
			}
		}

		return ProcessedList;
	}

	// ���������
	CTokenPtr ParseHeader(CTokenListCIter& i, CTokenListCIter e)
	{
		std::smatch MatchResult;
		// MatchResult [0] �ǽ���ƥ����ַ���
		//			   [1] ��ƥ��ɹ��Ĳ���
		//			   [2] �ǳ���ƥ��ɹ�ʣ�µĲ��� 
		if (!(*i)->IsBlankLine() && !(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			// #{1,6}(����) - ĩβ�Ƿ���#�հ�������ν
			static const std::regex HeaderRegex("^(#{1,6}) *(.*?) *#*$");

			if (std::regex_match((*i)->Text(), MatchResult, HeaderRegex))
			{
				return CTokenPtr(new Token::CHeader(MatchResult[1].length(), MatchResult[2]));
			}

			// (����)
			// =========/--------
			// ������̽ƥ�� - �ɹ��ǵ��޸�i��ֵ ʧ�ܾ�ֱ�ӷ���
			CTokenListCIter n = i;
			n++;
			if (n != e && !(*n)->IsBlankLine() && !(*n)->Text().empty() && (*i)->CanContainMarkup())
			{
				static const std::regex UnderlineHeaderRegex("^([-=])\\1*$");
				std::string Line = (*n)->Text();
				if (std::regex_match(Line, MatchResult, UnderlineHeaderRegex))
				{
					i = n;
					n--;
					return CTokenPtr(new Token::CHeader(std::string(MatchResult[1])[0] == '=' ? 1 : 2, (*n)->Text()));
				}
			}
		}
		return NULL;
	}

	// ����ָ���
	CTokenPtr ParseHorizontalRule(CTokenListCIter & i, CTokenListCIter e)
	{
		if (!(*i)->IsBlankLine() && !(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			static const std::regex HorizontalRuleRegex("^ {0,3}((?:-|\\*|_) *){3,}$");
			if (std::regex_match((*i)->Text(), HorizontalRuleRegex))
			{
				return CTokenPtr(new Token::CHtmlTag("hr/"));
			}
		}

		return CTokenPtr(NULL);
	}

	// �����б�
	CTokenPtr ParseListBlock(CTokenListCIter& i, CTokenListCIter e, bool bInline /* = false */)
	{
		static const std::regex UnorderListRegex("^( *)([*+-]) +([^*-].*)$");	// �ڿ�ͷƥ�� *|+|- �����ں��治����ƥ�䵽����� -|*  ---- �Ƿָ��� ** ��ǿ��
		static const std::regex OrderListRegex("^( *)([0-9]+)\\. +(.*)$");		// ��ͷƥ�� (����). ����ƥ�������ַ�����ĩβ
		std::regex NextItemRegex;
		std::smatch MatchResult;
		ListType Type = NoneList;
		size_t Indent = 0;	// �������� - ListҪ��С��4
		TCHAR StartChar = NULL;
		CTokenList SubItemTokens;	// List�ﻹ���Լ���ʹ�� Markdown�﷨ ����Ҫ��¼��List ʣ�²���ȥ�ٴ�ƥ��
		CTokenList Tokens;

		if (!(*i)->IsBlankLine() && !(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			if (std::regex_match((*i)->Text(), MatchResult, UnorderListRegex))
			{
				Indent = MatchResult[1].length();
				if (bInline || Indent < 4)
				{
					Type = UnorderList;
					StartChar = *(MatchResult[2].first);
					SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));

					std::ostringstream Next;
					Next << "^" << std::string(Indent, ' ') << "\\" << StartChar << " +([^*-].*)$";
					NextItemRegex = Next.str();
				}
			}
			else if (std::regex_match((*i)->Text(), MatchResult, OrderListRegex))
			{
				Indent = MatchResult[1].length();
				if (bInline || Indent < 4)
				{
					Type = OrderList;
					SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));

					std::ostringstream Next;
					Next << "^" << std::string(Indent, ' ') << "[0-9]+\\. +(.*)$";
					NextItemRegex = Next.str();
				}
			}

			// ���б�������ʽƥ��ʽ
			static const std::regex SubListRegex("^" + std::string(Indent, ' ') + " +(([*+-])|([0-9]+\\.)) +.*$");

			static const std::regex ContinuedAfterBlankLineRegex("^ {" + std::to_string(4 + Indent) + "}([^ ].*)$");	// �������ɲ���
			static const std::regex CodeBlockAfterBlankLineRegex("^ {" + std::to_string(8 + Indent) + "}(.*)$");		// CodeBlock
			static const std::regex CurItemContinuedRegex("^ *([^ ].*)$");

			if (Type != NoneList)
			{
				CTokenListCIter OriginalIter = i;
				// ����ƥ�䵽List����һ�У������µĿ�����
				// 1. ���ǵ�ǰ�������
				// 2. ��ǰList����һ��
				// 3. ����List�ĵ�һ��
				// 4. �Ǹ�List����һ��
				// 5. ����

				// ����ǿ��У� �Ǿͻ���Ҫ���¿�һ�С���ΪList��ÿ����֮���������
				// ����һ������ǵ�ǰ�б����Ŀ�����䲢�뵱ǰ�Σ���������
				// ����һ�������4�����ϵ����������ǵ�ǰ��Ŀ�����ⲿ�� - codeblock/rawtext - �ȴ��ٴ�ƥ��
				// ���߾�˵�� ��ǰList�Ѿ�����������һ�����ݻ���һ�����б�Ŀ�ʼ
				i++;
				NextLineListType NextType = NoneNextList;
				bool bInitList = false;
				bool ListParagraphMode = false;
				while (i != e)
				{
					// ��һ���ǿ���
					if ((*i)->IsBlankLine())
					{
						i++;
						if (i == e)
						{
							NextType = ListEnd;
						}
						else if (!(*i)->Text().empty())
						{
							// �����б�Ŀ�ʼ�� - �ݹ�
							if (std::regex_match((*i)->Text(), SubListRegex))
							{
								ListParagraphMode = true;
								bInitList = true;
								CTokenPtr Return = ParseListBlock(i, e, true);
								SubItemTokens.push_back(Return);
								continue;
							}
							// �ǵ�ǰList����һ��Item ��
							else if (std::regex_match((*i)->Text(), MatchResult, NextItemRegex))
							{
								ListParagraphMode = true;
								NextType = NextCurListItem;
							}
							// ӵ��4�����ϵ������� - ��ȡ������ΪRawText����ȴ��´δ���
							else if (std::regex_match((*i)->Text(), MatchResult, ContinuedAfterBlankLineRegex))
							{
								SubItemTokens.push_back(CTokenPtr(new Token::CBlankLine()));
								SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
								i++;	// ��������
								continue;
							}
							// ��Ƕ CodeBlock �� ��ΪCodeBlock �����
							else if (std::regex_match((*i)->Text(), MatchResult, CodeBlockAfterBlankLineRegex))
							{
								ListParagraphMode = true;
								bInitList = true;
								SubItemTokens.push_back(CTokenPtr(new Token::CBlankLine()));

								std::string CodeBlock = std::string(MatchResult[1]) + "\n";
								i++;
								CTokenListCIter n = i;
								// CodeBlock ������ List  �����������while�������ƵĴ���
								while (n != e)
								{
									// ���� + CodeBlock
									if ((*n)->IsBlankLine())
									{
										n++;
										if (std::regex_match((*n)->Text(), MatchResult, CodeBlockAfterBlankLineRegex))
										{
											CodeBlock += '\n' + std::string(MatchResult[1]) + '\n';
										}
										else
										{
											n--;
											break;
										}
									}
									// ������CodeBlock��
									else if (!(*n)->Text().empty())
									{
										if (std::regex_match((*i)->Text(), MatchResult, CodeBlockAfterBlankLineRegex))
										{
											CodeBlock += std::string(MatchResult[1]) + '\n';
										}
										else
										{
											break;
										}
									}
									else
									{
										break;
									}
									n++;
								}
								i = n;
								SubItemTokens.push_back(CTokenPtr(new Token::CCodeBlock(CodeBlock)));
								continue;
							}
							else
							{
								i--;
								NextType = ListEnd;
							}
						}
						else
						{
							break;
						}
					}
					else if (!(*i)->Text().empty())
					{
						// ���б�Ŀ�ʼ - �ݹ�
						if (std::regex_match((*i)->Text(), SubListRegex))
						{
							bInitList = true;
							CTokenPtr Return = ParseListBlock(i, e, true);
							SubItemTokens.push_back(Return);
							continue;
						}
						// ��ǰList����һ��
						else if (std::regex_match((*i)->Text(), MatchResult, NextItemRegex))
						{
							NextType = NextCurListItem;
						}
						else
						{
							// ��List����һ��
							if (std::regex_match((*i)->Text(), MatchResult, UnorderListRegex) ||
								std::regex_match((*i)->Text(), MatchResult, OrderListRegex))
							{
								NextType = ListEnd;
							}
							else
							{
								std::regex_match((*i)->Text(), MatchResult, CurItemContinuedRegex);
								SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
								i++;
								continue;
							}
						}
					}
					else
					{
						NextType = ListEnd;
					}

					// ��ǰ��һ�е��������ݷ���Tokens
					if (!SubItemTokens.empty())
					{
						Tokens.push_back(CTokenPtr(new Token::CListItem(SubItemTokens)));
						SubItemTokens.clear();
					}

					if (NextType == NextCurListItem)
					{
						bInitList = true;
						SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
						i++;
					}
					else/*NextType == ListEnd*/
					{
						break;
					}
				}

				// Ԥ��
				if (!SubItemTokens.empty())
				{
					Tokens.push_back(CTokenPtr(new Token::CListItem(SubItemTokens)));
					SubItemTokens.clear();
				}

				if (bInitList || Indent != 0)
				{
					if (Type == UnorderList)
					{
						return CTokenPtr(new Token::CUnorderedList(Tokens, ListParagraphMode));
					}
					else if (Type == OrderList)
					{
						return CTokenPtr(new Token::COrderedList(Tokens, ListParagraphMode));
					}
				}
				else // ƥ�䲻���� �˻�ԭ��ʼλ��
				{
					i = OriginalIter;
					return CTokenPtr(NULL);
				}
			}
		}

		return CTokenPtr();
	}

	// ��������
	CTokenPtr ParseBlockQuote(CTokenListCIter& i, CTokenListCIter e)
	{
		static const std::regex BlockQuoteRegex("^((?: {0,3}>)+) (.*)$");	// ������0~3�ո񣬵����ǲ�����Ȼ����һ������>���ո񣬲�����������
	
		if (!(*i)->IsBlankLine() && !(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			std::smatch MatchResult;
			size_t QuoteLevel = 0;
			if (std::regex_match((*i)->Text(), MatchResult, BlockQuoteRegex))
			{
				QuoteLevel = CountQuoteLevel(MatchResult[1]);
				static const std::regex ContinuationQuoteRegex("^((?: {0,3}>){" + std::to_string(QuoteLevel) + "}) ?(.*)$");
				const static std::regex continuousQuoteRegex("^(.*)$");

				CTokenList Tokens;
				Tokens.push_back(CTokenPtr(new Token::CParagraph( CTokenPtr(new Token::CRawText(MatchResult[2])) )));

				// ���õ���һ��
				// 1. ���� > �����ò���
				// 2. ������ > �����ò���
				// 3. ����
				// ������Ҫ�������� - ��������һ��
				// ��ȷƥ�� BlockQuote - ��Ϊ��ǰ BlockQuote
				// ��ƥ�� ������ǰ BlockQuote

				i++;
				while (i != e)
				{
					if ((*i)->IsBlankLine())
					{
						i++; // �ؼ��� ���е���һ��
						if (i == e)
						{
							break;
						}
						else
						{
							// ���и��� ������ >
							if (std::regex_match((*i)->Text(), MatchResult, ContinuationQuoteRegex))
							{
								if (MatchResult[1].matched && MatchResult[1].length() > 0)
								{
									i++;
									Tokens.push_back(CTokenPtr(new Token::CBlankLine()));
									Tokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[2])));
								}
								else
								{
									i--;
									break;
								}
							}
							else
							{
								i--;
								break;
							}
						}
					}
					// ���ǿ���
					else
					{					
						// ��һ�п�ͷ�� > ?
						if (std::regex_match((*i)->Text(), MatchResult, ContinuationQuoteRegex))
						{
							if (!IsBlankLine(MatchResult[2]))
							{
								Tokens.push_back(CTokenPtr(new Token::CParagraph(CTokenPtr(new Token::CRawText(MatchResult[2])))));
							}
							// ����
							else
							{
								Tokens.push_back(CTokenPtr(new Token::CParagraph(CTokenPtr(new Token::CBlankLine(MatchResult[2])))));
							}
							i++;
						}
						// ������ > ��Ӧ��������
						else if (std::regex_match((*i)->Text(), MatchResult, continuousQuoteRegex))
						{
							Tokens.push_back(CTokenPtr(new Token::CParagraph(CTokenPtr(new Token::CRawText(MatchResult[1])))));
							i++;
						}
						else
						{
							break;
						}
					}	
				}
				return CTokenPtr(new Token::CBlockQuote(Tokens));
			}
		}
		return NULL;
	}

	// ���� CodeBlock
	CTokenPtr ParseCodeBlock(CTokenListCIter& i, CTokenListCIter e)
	{
		if (!(*i)->IsBlankLine())
		{
			std::string Contents = IsCodeBlockLine(i, e);
			if (!Contents.empty())
			{
				std::ostringstream Out;
				Out << Contents << '\n';
				while (i != e)
				{
					Contents = IsCodeBlockLine(i, e);
					if (!Contents.empty())
					{
						Out << Contents << '\n';
					}
					else
					{
						break;
					}
				}

				return CTokenPtr(new Token::CCodeBlock(Out.str()));
			}
		}
		return CTokenPtr();
	}

	std::string IsCodeBlockLine(Markdown::CTokenListCIter& i, Markdown::CTokenListCIter e)
	{
		if ((*i)->IsBlankLine())
		{
			i++;
			if (i != e)
			{
				std::string Result = IsCodeBlockLine(i, e);
				if (!Result.empty())
				{
					return std::string("\n" + Result);
				}
			}
			i--;
		}
		else if (!(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			const std::string& Line = (*i)->Text();
			if (Line.length() >= 4)
			{
				// ֻ�е�ǰ�����ĸ� �ո񣬾���Ϊ�� CodeBlock
				
				std::string::const_iterator strIter = Line.begin();
				std::string::const_iterator strEndIter = strIter + 4;
				while (strIter != strEndIter && *strIter == ' ')
				{
					strIter++;
				}
				if (strIter == strEndIter)
				{
					i++;
					return std::string(strIter, Line.end());  // string iterators incompatible ??? 
				}
			}
		}

		return std::string();
	}
	
}

namespace Markdown::Token
{
	//						CTextHoler 
	// CRawText	CHeader CBlankLine CHeader CHtmlTag

	// CTextHolder
	CTextHolder::CTextHolder(const std::string& Text, bool CanContainMarkup, ULONG TranscodingFlags) :
		m_Text(Text), m_bCanContainMarkup(CanContainMarkup), m_TranscodingFlags(TranscodingFlags)
	{

	}

	void CTextHolder::WriteAsHTML(std::ostream& Out) const
	{
		HeaderWrite(Out);
		if (m_TranscodingFlags != 0)
		{
			Out << TransCharacter(m_Text, m_TranscodingFlags);
		}
		else
		{
			Out << m_Text;
		}

		TailWrite(Out);
	}

	// CRawTetx
	CRawText::CRawText(const std::string Text, bool bCanContainMarkup /* = true */) :
		CTextHolder(Text, bCanContainMarkup, SelectTransferAmps | TransferCompar)
	{

	}

	CTokenList CRawText::ProcessSectorElements()
	{
		CTokenList ProcessedList;
		std::string ProcessString;
		// ��������Ԫ��
		// 1.�������� - ͼƬ
		_ProcessInterlink(Text(), ProcessedList);
		// 2.б���ǿ��
		_ProcessEmphasis(ProcessedList);
		// 3.�̴���
		_ProcessInlineCode(ProcessedList);

		return CTokenList(ProcessedList);
	}

	void CRawText::_ProcessInterlink(std::string SourText, CTokenList& ProcessedList)
	{
		// ����������ʽ
		// 1.����ʽ
		// 2.�ο�ʽ
		// (1) - ǰ�����ݺ�
		// (2) - []�ڵ����ݣ���������
		// (3) - ()�ڵ�����, Ӧ����Title��URL
		const std::regex InlineInterLinkRegex("^([^!]*)\\[([^\\]]+?)\\]\\((.*?)\\)(.*)$");
		const std::regex ReferenceInterLinkRegex("^([^!]*)\\[([^\\]]+?)\\] ?\\[(.*?)\\](.*)$");
		const std::regex InlineImageLinkRegex("^(.*)!\\[([^\\]]+?)\\]\\((.*?)\\)(.*)$");
		const std::regex ReferenceImageLinkRegex("^(.*)!\\[([^\\]]+?)\\]\\[([^\\]]+?)\\](.*)$");

		const std::regex TitleRegex("^([^ ]*)(?: *(?:('|\")(.*)\\2)|(?:\\((.*)\\)))? *$");	// ��������URL Title
		std::smatch MatchResult;
		std::smatch TitleMatch;
		std::string::const_iterator i = SourText.begin(), e = SourText.end();
		while (i != e)
		{
			// ����ʽ
			if (std::regex_match(i, e, MatchResult, InlineInterLinkRegex, std::regex_constants::match_any))
			{	
				// �ݹ���� �ж�ǰ���Ƿ��� ƥ����
				_ProcessInterlink(std::string(i, i + MatchResult[1].length()), ProcessedList);
				i = MatchResult[4].first;	// ���Ƶ� ���ӽ����ĵط�
				std::string strContent = MatchResult[3];
				std::string strTitle;
				std::string strURL;
				
				// �ж��Ƿ���title
				if (std::regex_match(strContent, TitleMatch, TitleRegex))
				{
					strURL = TitleMatch[1];
					if (TitleMatch[3].matched)
					{
						strTitle = TitleMatch[3];
					}
					else if (TitleMatch[4].matched)
					{
						strTitle = TitleMatch[4];
					}
				}
				ProcessedList.push_back(CTokenPtr(new Token::CHtmlLinkTag((strURL.empty() ? strContent: strURL), strTitle)));
				ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[2])));
				ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/a")));
				continue;
			}
			else if (std::regex_match(i, e, MatchResult, ReferenceInterLinkRegex))
			{
				i = MatchResult[4].first;

				continue;
			}
			else if (std::regex_match(i, e, MatchResult, InlineImageLinkRegex))
			{
				_ProcessInterlink(std::string(i, i + MatchResult[1].length()), ProcessedList);
				i = MatchResult[4].first;
				std::string strContent = MatchResult[3];
				std::string strURL;
				std::string strTitle;	

				if (std::regex_match(strContent, TitleMatch, TitleRegex))
				{
					strURL = TitleMatch[1];
					if (TitleMatch[3].matched)
					{
						strTitle = TitleMatch[3];
					}
					else if (TitleMatch[4].matched)
					{
						strTitle = TitleMatch[4];
					}
				}
				ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
				ProcessedList.push_back(CTokenPtr(new Token::CHtmlImgTag((strURL.empty() ? strContent : strURL), MatchResult[2], strTitle)));
				ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/img")));
				continue;
			}
			else if (std::regex_match(i, e, MatchResult, ReferenceImageLinkRegex))
			{
				i = MatchResult[4].first;

				continue;
			}
			else
			{
				
				ProcessedList.push_back(CTokenPtr(new Token::CRawText(std::string(i, e))));
				break;
			}
			i++;
		}
	}

	void CRawText::_ProcessEmphasis(CTokenList & SourList)
	{
		const static std::regex EmphasisRegex("^([^*]*)([*_]{1,2})([^*]+)([*_]{1,2})(.*)");
		std::smatch MatchResult;
		CTokenList ProcessedList;
		int count = 0;
		for (CTokenListIter i = SourList.begin(); i != SourList.end(); i++)
		{
			if ((*i)->IsContainer())
			{
				// ��Ӧ�ý���
				return;
			}
			count = 0;
			for (std::string::const_iterator strIter = (*i)->Text().begin(); strIter != (*i)->Text().end(); count++)
			{
				if (std::regex_match(strIter, (*i)->Text().end(),MatchResult, EmphasisRegex))
				{
					strIter = MatchResult[5].first;
					// �ж� �Ƿ��������� * - ֻҪһ�� * ǰ���� \ ��ȡ����Լ
					if (std::string(MatchResult[1]).c_str()[MatchResult[1].length()] == '\\' ||
						std::string(MatchResult[3]).c_str()[MatchResult[3].length()] == '\\')
					{
						continue;
					}

					// �ж���������ǰ���Ƿ����пհ� - ǰ�������� �˳���Լ
					if ((std::string(MatchResult[1]).c_str()[MatchResult[1].length()] == ' '  &&
						 std::string(MatchResult[3]).c_str()[0] == ' ') ||
						(std::string(MatchResult[3]).c_str()[MatchResult[3].length()] == ' '  &&
						 std::string(MatchResult[5]).c_str()[0] == ' '))
					{
						continue;
					}
					// �ж��Ƿ�ǰ�����
					if (MatchResult[2].length() != MatchResult[4].length())
					{
						continue;
					}

					// ����ǿ���̶�
					if (MatchResult[2].length() == 1)
					{
						ProcessedList.push_back(CTokenPtr(new CRawText(MatchResult[1])));
						ProcessedList.push_back(CTokenPtr(new CHtmlTag("em")));
						ProcessedList.push_back(CTokenPtr(new CRawText(MatchResult[3])));
						ProcessedList.push_back(CTokenPtr(new CHtmlTag("/em")));
						continue;
					}
					else if (MatchResult[2].length() >= 2)
					{
						ProcessedList.push_back(CTokenPtr(new CRawText(MatchResult[1])));
						ProcessedList.push_back(CTokenPtr(new CHtmlTag("strong")));
						ProcessedList.push_back(CTokenPtr(new CRawText(MatchResult[3])));
						ProcessedList.push_back(CTokenPtr(new CHtmlTag("/strong")));
						continue;
					}
				}
				else
				{
					if (strIter == (*i)->Text().begin())
					{
						ProcessedList.push_back(*i);
					}
					else
					{
						ProcessedList.push_back(CTokenPtr(new CRawText(std::string(strIter, (*i)->Text().end()))));
					}
					
					break;
				}
			}		
		}
		SourList.swap(ProcessedList);
	}

	void CRawText::_ProcessInlineCode(CTokenList& SourList)
	{
		const static std::regex InlineCodeRegex("^([^`]*)(`{1,})([^`]+)(`{1,})(.*)$");		// ����Ҫ�ж� ` ����ʼ���� ��� `��1�� ��ô�������Ϳ�������ж� -
																					    // ������� ���Ǿ�Ҫ�� �����ҵ��ĺ�벿�ֵ� ` ��û�и�ǰ���Ӧ �������Ӧ���ǻ�Ҫ�Լ�������
		std::smatch MatchResult;
		std::smatch InlineTailResult;
		CTokenList ProcessedList;

		for (CTokenListIter i = SourList.begin(); i != SourList.end(); i++)
		{
			for (std::string::const_iterator strIter = (*i)->Text().begin(); strIter != (*i)->Text().end(); )
			{
				if (std::regex_match(strIter, (*i)->Text().end(), MatchResult, InlineCodeRegex))
				{
					strIter = MatchResult[5].first;
					// ��򵥵���� �����ҵ���ǰ�� ` ������ͬ ����һ����������Ƕcode
					if (MatchResult[2].length() == MatchResult[4].length())
					{
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
						ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("code")));
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));
						ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/code")));
						continue;
					}
					// ǰ���������ͬ ����ֻ���� ǰ��������ں�������
					else if (MatchResult[2].length() > MatchResult[4].length())
					{
						// �õ�ǰ�� ` �ĸ��� �������µ�������ʽ ������ �ȸ��� `
						std::regex InlineCodeTailRegex("([^`]+)(`{" + std::to_string(MatchResult[2].length()) + "})(.*)");
						if (std::regex_match(MatchResult[5].first, (*i)->Text().end(), InlineTailResult, InlineCodeTailRegex))
						{
							// �ҵ���
							strIter = InlineTailResult[3].first;
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
							ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("code")));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[4])));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(InlineTailResult[1])));
							ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/code")));
							continue;
						}
						// û���ҵ�ƥ���	
						if (strIter == (*i)->Text().begin())
						{
							ProcessedList.push_back(*i);
						}
						else
						{
							ProcessedList.push_back(CTokenPtr(new CRawText(std::string(strIter, (*i)->Text().end()))));
						}
					}
					else
					{
						strIter = MatchResult[3].first;
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[2])));
					}
				}
				else
				{
					if (strIter == (*i)->Text().begin())
					{
						ProcessedList.push_back(*i);
					}
					else
					{
						ProcessedList.push_back(CTokenPtr(new CRawText(std::string(strIter, (*i)->Text().end()))));
					}

					break;
				}
			}
		}
		SourList.swap(ProcessedList);
	}
	
	//CBlankLine
	CBlankLine::CBlankLine(const std::string& ActualText /* = std::string() */) :
		CTextHolder(ActualText, false, 0)
	{

	}

	bool CBlankLine::IsBlankLine() const
	{
		return true;
	}

	//CHeader
	CHeader::CHeader(size_t Level, const std::string& Text) :
		CTextHolder(Text, true, SelectTransferAmps | TransferCompar), m_Level(Level)
	{

	}

	void CHeader::HeaderWrite(std::ostream& Out) const
	{
		Out << "<h" << m_Level << ">";
	}

	void CHeader::TailWrite(std::ostream& Out) const
	{
		Out << "</h" << m_Level << ">\r\n";
	}

	//CHtmlTag
	CHtmlTag::CHtmlTag(const std::string& Text) :
		CTextHolder(Text, false, SelectTransferAmps | TransferCompar)
	{

	}

	void CHtmlTag::HeaderWrite(std::ostream& Out) const
	{
		Out << "<";
	}

	void CHtmlTag::TailWrite(std::ostream& Out) const
	{
		Out << ">\r\n";
	}

	// CodeBlock
	CCodeBlock::CCodeBlock(const std::string& Contents) :
		CTextHolder(Contents, false, TransferAmps | TransferCompar)
	{

	}

	void CCodeBlock::WriteAsHTML(std::ostream & Out) const
	{
		Out << "<pre><code>";
		CTextHolder::WriteAsHTML(Out);
		Out << "</code></pre>\n\n";
	}

	CHtmlLinkTag::CHtmlLinkTag(const std::string & URL, std::string & Title):
		CTextHolder("<a href=\"" + TransCharacter(URL, SelectTransferAmps) + "\"" + 
				   (Title.empty() ? std::string() : " title=\"" + TransCharacter(Title, SelectTransferAmps) + "\"") + 
			       ">", false, 0)

	{

	}

	CHtmlImgTag::CHtmlImgTag(const std::string & SRC, const std::string & ALT, std::string & Title) :
		CTextHolder("<img src=\"" + TransCharacter(SRC, SelectTransferAmps) + "\"" +
					" alt=\"" + TransCharacter(ALT, SelectTransferAmps) + "\"" + 
					(Title.empty() ? std::string() : " title=\"" + TransCharacter(Title, SelectTransferAmps) + "\"") +
					">", false, 0)
	{
	}

}

namespace Markdown::Token
{
	//						CContainer 
	//CListItem CUnorderedList COrderedList
	CContainer::CContainer(const CTokenList& Contents /* = CTokenList() */) :
		m_SubTokens(Contents), m_bParagraphMode(false)
	{

	}

	const CTokenList& CContainer::SubTokens() const
	{
		return m_SubTokens;
	}

	const std::string & CContainer::Text() const
	{
		if (!m_SubTokens.empty())
		{
			return (*m_SubTokens.begin())->Text();
		}

		return std::string(NULL);
	}

	void CContainer::WriteAsHTML(std::ostream & Out) const
	{
		HeaderWrite(Out);
		for (CTokenListCIter i = m_SubTokens.begin(); i != m_SubTokens.end(); i++)
		{
			(*i)->WriteAsHTML(Out);
		}
		TailWrite(Out);
	}

	CTokenList CContainer::ProcessSectorElements()
	{
		CTokenList ProcessedList;
		for (CTokenListCIter i = m_SubTokens.begin(); i != m_SubTokens.end(); i++)
		{
			if (!(*i)->Text().empty())
			{
				CTokenList SubTokens = (*i)->ProcessSectorElements();
				if (!SubTokens.empty())
				{
					if (SubTokens.size() > 1)
					{
						ProcessedList.push_back(CTokenPtr(new Token::CContainer(SubTokens)));
					}
					else
					{
						ProcessedList.push_back(*SubTokens.begin());
					}
				}
				else
				{
					ProcessedList.push_back(*i);
				}
			}
			else
			{
				ProcessedList.push_back(*i);
			}
		}

		SwapSubTokens(ProcessedList);
		return CTokenList();
	}

	void CContainer::AddSubTokens(CTokenList& Tokens)
	{
		m_SubTokens.splice(m_SubTokens.end(), Tokens);
		return;
	}

	void CContainer::SwapSubTokens(CTokenList& Tokens)
	{
		m_SubTokens.swap(Tokens);
		return;
	}

	// CListItem
	CListItem::CListItem(const CTokenList& Contents) :
		CContainer(Contents), m_InhibitParagraphs(true)
	{

	}

	void CListItem::InhibitParagraphs(bool Value)
	{
		m_InhibitParagraphs = Value;
	}

	bool CListItem::InhibitParagraphs() const
	{
		return m_InhibitParagraphs;
	}

	void CListItem::HeaderWrite(std::ostream& Out) const
	{
		Out << "<li>";
	}

	void CListItem::TailWrite(std::ostream& Out) const
	{
		Out << "</li>\n";
	}

	// CUnorderedList
	CUnorderedList::CUnorderedList(const CTokenList& Contents, bool bAsParagraphMode /* = false */)
	{
		if (bAsParagraphMode)
		{
			for (CTokenListCIter i = Contents.begin(); i != Contents.end(); i++)
			{
				Token::CListItem* Item = dynamic_cast<Token::CListItem*>((*i).get());
				Item->InhibitParagraphs(false);
				m_SubTokens.push_back(*i);
			}
		}
		else
		{
			m_SubTokens = Contents;
		}
	}

	void CUnorderedList::HeaderWrite(std::ostream& Out) const
	{
		Out << "\n<ul>\n";
	}

	void CUnorderedList::TailWrite(std::ostream& Out) const
	{
		Out << "</ul>\n\n";
	}

	// COrderedList
	COrderedList::COrderedList(const CTokenList& Contents, bool bAsParagraphMode /* = false */) :
		CUnorderedList(Contents, bAsParagraphMode)
	{

	}

	void COrderedList::HeaderWrite(std::ostream& Out) const
	{
		Out << "<ol>\n";
	}

	void COrderedList::TailWrite(std::ostream& Out) const
	{
		Out << "</ol>\n\n";
	}

	// CBlockQuote
	CBlockQuote::CBlockQuote(const CTokenList& Contents) :
		CContainer(Contents)
	{

	}

	void CBlockQuote::HeaderWrite(std::ostream& Out) const
	{
		Out << "<blockquote>\n";
	}

	void CBlockQuote::TailWrite(std::ostream& Out) const
	{
		Out << "\b</blockquote>\n";
	}

	// CParagraph
	CParagraph::CParagraph(const CTokenList& Contents) :
		CContainer(Contents)
	{

	}

	CParagraph::CParagraph(const CTokenPtr& ParagrapnText) :
		CContainer()
	{
		m_SubTokens.push_back(ParagrapnText);
	}

	void CParagraph::HeaderWrite(std::ostream & Out) const
	{
		Out << "<p>";
	}

	void CParagraph::TailWrite(std::ostream& Out) const
	{
		Out << "</p>\n\n";
	}
}