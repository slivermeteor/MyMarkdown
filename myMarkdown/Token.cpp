#include "Token.h"

namespace Markdown
{
	// 处理特殊字符
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
				// 在下列情况下不转换 & - 1.&amp 2.16位进位码
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

	// 处理标题栏
	CTokenPtr ParseHeader(CTokenListCIter& i, CTokenListCIter e)
	{
		std::smatch MatchResult;
		// MatchResult [0] 是进行匹配的字符串
		//			   [1] 是匹配成功的部分
		//			   [2] 是除了匹配成功剩下的部分 
		if (!(*i)->IsBlankLine() && !(*i)->Text().empty() && (*i)->CanContainMarkup())
		{
			// #{1,6}(标题) - 末尾是否有#闭包，无所谓
			static const std::regex HeaderRegex("^(#{1,6}) *(.*?) *#*$");

			if (std::regex_match((*i)->Text(), MatchResult, HeaderRegex))
			{
				return CTokenPtr(new Token::CHeader(MatchResult[1].length(), MatchResult[2]));
			}

			// (标题)
			// =========/--------
			// 向下试探匹配 - 成功记得修改i的值 失败就直接返回
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

	// 处理分割线
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

	// 处理列表
	CTokenPtr ParseListBlock(CTokenListCIter& i, CTokenListCIter e, bool bInline /* = false */)
	{
		static const std::regex UnorderListRegex("^( *)([*+-]) +([^*-].*)$");	// 在开头匹配 *|+|- 但是在后面不能在匹配到更多的 -|*  ---- 是分割线 ** 是强调
		static const std::regex OrderListRegex("^( *)([0-9]+)\\. +(.*)$");		// 开头匹配 (数字). 后面匹配任意字符到行末尾
		std::regex NextItemRegex;
		std::smatch MatchResult;
		ListType Type = NoneList;
		size_t Indent = 0;	// 缩进长度 - List要求小于4
		TCHAR StartChar = NULL;
		CTokenList SubItemTokens;	// List里还可以继续使用 Markdown语法 所以要记录下List 剩下部分去再次匹配
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

			// 子列表正则表达式匹配式
			static const std::regex SubListRegex("^" + std::string(Indent, ' ') + " +(([*+-])|([0-9]+\\.)) +.*$");

			static const std::regex ContinuedAfterBlankLineRegex("^ {" + std::to_string(4 + Indent) + "}([^ ].*)$");	// 另外的组成部分
			static const std::regex CodeBlockAfterBlankLineRegex("^ {" + std::to_string(8 + Indent) + "}(.*)$");		// CodeBlock
			static const std::regex CurItemContinuedRegex("^ *([^ ].*)$");

			if (Type != NoneList)
			{
				CTokenListCIter OriginalIter = i;
				// 对于匹配到List的下一行，有以下的可能性
				// 1. 还是当前项的内容
				// 2. 当前List的下一项
				// 3. 是子List的第一项
				// 4. 是父List的下一项
				// 5. 空行

				// 如果是空行， 那就还需要往下看一行。因为List的每个项之间允许空行
				// 再下一行如果是当前列表的项目，则将其并入当前段，继续处理。
				// 再下一行如果有4个以上的缩进，则是当前条目的另外部分 - codeblock/rawtext - 等待再次匹配
				// 或者就说明 当前List已经结束，是下一个内容或者一个子列表的开始
				i++;
				NextLineListType NextType = NoneNextList;
				bool bInitList = false;
				bool ListParagraphMode = false;
				while (i != e)
				{
					// 下一行是空行
					if ((*i)->IsBlankLine())
					{
						i++;
						if (i == e)
						{
							NextType = ListEnd;
						}
						else if (!(*i)->Text().empty())
						{
							// 是子列表的开始行 - 递归
							if (std::regex_match((*i)->Text(), SubListRegex))
							{
								ListParagraphMode = true;
								bInitList = true;
								CTokenPtr Return = ParseListBlock(i, e, true);
								SubItemTokens.push_back(Return);
								continue;
							}
							// 是当前List的下一个Item ？
							else if (std::regex_match((*i)->Text(), MatchResult, NextItemRegex))
							{
								ListParagraphMode = true;
								NextType = NextCurListItem;
							}
							// 拥有4个以上的缩进？ - 提取内容作为RawText放入等待下次处理
							else if (std::regex_match((*i)->Text(), MatchResult, ContinuedAfterBlankLineRegex))
							{
								SubItemTokens.push_back(CTokenPtr(new Token::CBlankLine()));
								SubItemTokens.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
								i++;	// 迈过空行
								continue;
							}
							// 内嵌 CodeBlock ？ 作为CodeBlock 处理掉
							else if (std::regex_match((*i)->Text(), MatchResult, CodeBlockAfterBlankLineRegex))
							{
								ListParagraphMode = true;
								bInitList = true;
								SubItemTokens.push_back(CTokenPtr(new Token::CBlankLine()));

								std::string CodeBlock = std::string(MatchResult[1]) + "\n";
								i++;
								CTokenListCIter n = i;
								// CodeBlock 很类似 List  所以下面这个while进行类似的处理
								while (n != e)
								{
									// 空行 + CodeBlock
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
									// 连续的CodeBlock行
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
						// 子列表的开始 - 递归
						if (std::regex_match((*i)->Text(), SubListRegex))
						{
							bInitList = true;
							CTokenPtr Return = ParseListBlock(i, e, true);
							SubItemTokens.push_back(Return);
							continue;
						}
						// 当前List的下一项
						else if (std::regex_match((*i)->Text(), MatchResult, NextItemRegex))
						{
							NextType = NextCurListItem;
						}
						else
						{
							// 父List的下一行
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

					// 当前这一行的所有内容放入Tokens
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

				// 预防
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
				else // 匹配不完整 退回原起始位置
				{
					i = OriginalIter;
					return CTokenPtr(NULL);
				}
			}
		}

		return CTokenPtr();
	}

	// 处理引用
	CTokenPtr ParseBlockQuote(CTokenListCIter& i, CTokenListCIter e)
	{
		static const std::regex BlockQuoteRegex("^((?: {0,3}>)+) (.*)$");	// 行首有0~3空格，但我们不捕获，然后是一个以上>。空格，捕获所有内容
	
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

				// 引用的下一行
				// 1. 带有 > 的引用部分
				// 2. 不带有 > 的引用部分
				// 3. 空行
				// 空行又要单独处理 - 根据其下一行
				// 正确匹配 BlockQuote - 作为当前 BlockQuote
				// 不匹配 结束当前 BlockQuote

				i++;
				while (i != e)
				{
					if ((*i)->IsBlankLine())
					{
						i++; // 关键看 空行的下一行
						if (i == e)
						{
							break;
						}
						else
						{
							// 空行隔开 必须有 >
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
					// 不是空行
					else
					{					
						// 下一行开头有 > ?
						if (std::regex_match((*i)->Text(), MatchResult, ContinuationQuoteRegex))
						{
							if (!IsBlankLine(MatchResult[2]))
							{
								Tokens.push_back(CTokenPtr(new Token::CParagraph(CTokenPtr(new Token::CRawText(MatchResult[2])))));
							}
							// 空行
							else
							{
								Tokens.push_back(CTokenPtr(new Token::CParagraph(CTokenPtr(new Token::CBlankLine(MatchResult[2])))));
							}
							i++;
						}
						// 不带有 > 则应该是连续
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

	// 处理 CodeBlock
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
				// 只有当前面有四个 空格，就认为是 CodeBlock
				
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
		// 处理区段元素
		// 1.处理链接 - 图片
		_ProcessInterlink(Text(), ProcessedList);
		// 2.斜体和强调
		_ProcessEmphasis(ProcessedList);
		// 3.短代码
		_ProcessInlineCode(ProcessedList);

		return CTokenList(ProcessedList);
	}

	void CRawText::_ProcessInterlink(std::string SourText, CTokenList& ProcessedList)
	{
		// 两种链接形式
		// 1.行内式
		// 2.参考式
		// (1) - 前面内容和
		// (2) - []内的内容，链接文字
		// (3) - ()内的内容, 应该是Title和URL
		const std::regex InlineInterLinkRegex("^([^!]*)\\[([^\\]]+?)\\]\\((.*?)\\)(.*)$");
		const std::regex ReferenceInterLinkRegex("^([^!]*)\\[([^\\]]+?)\\] ?\\[(.*?)\\](.*)$");
		const std::regex InlineImageLinkRegex("^(.*)!\\[([^\\]]+?)\\]\\((.*?)\\)(.*)$");
		const std::regex ReferenceImageLinkRegex("^(.*)!\\[([^\\]]+?)\\]\\[([^\\]]+?)\\](.*)$");

		const std::regex TitleRegex("^([^ ]*)(?: *(?:('|\")(.*)\\2)|(?:\\((.*)\\)))? *$");	// 用来区分URL Title
		std::smatch MatchResult;
		std::smatch TitleMatch;
		std::string::const_iterator i = SourText.begin(), e = SourText.end();
		while (i != e)
		{
			// 行内式
			if (std::regex_match(i, e, MatchResult, InlineInterLinkRegex, std::regex_constants::match_any))
			{	
				// 递归调用 判断前面是否还有 匹配项
				_ProcessInterlink(std::string(i, i + MatchResult[1].length()), ProcessedList);
				i = MatchResult[4].first;	// 下移到 链接结束的地方
				std::string strContent = MatchResult[3];
				std::string strTitle;
				std::string strURL;
				
				// 判断是否有title
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
				// 不应该进来
				return;
			}
			count = 0;
			for (std::string::const_iterator strIter = (*i)->Text().begin(); strIter != (*i)->Text().end(); count++)
			{
				if (std::regex_match(strIter, (*i)->Text().end(),MatchResult, EmphasisRegex))
				{
					strIter = MatchResult[5].first;
					// 判断 是否是想输入 * - 只要一个 * 前面有 \ 就取消规约
					if (std::string(MatchResult[1]).c_str()[MatchResult[1].length()] == '\\' ||
						std::string(MatchResult[3]).c_str()[MatchResult[3].length()] == '\\')
					{
						continue;
					}

					// 判断两个符号前后是否留有空白 - 前后都由留白 退出规约
					if ((std::string(MatchResult[1]).c_str()[MatchResult[1].length()] == ' '  &&
						 std::string(MatchResult[3]).c_str()[0] == ' ') ||
						(std::string(MatchResult[3]).c_str()[MatchResult[3].length()] == ' '  &&
						 std::string(MatchResult[5]).c_str()[0] == ' '))
					{
						continue;
					}
					// 判断是否前后相等
					if (MatchResult[2].length() != MatchResult[4].length())
					{
						continue;
					}

					// 计算强调程度
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
		const static std::regex InlineCodeRegex("^([^`]*)(`{1,})([^`]+)(`{1,})(.*)$");		// 这里要判断 ` 的起始个数 如果 `是1个 那么这个正则就可以完成判断 -
																					    // 如果不是 我们就要看 我们找到的后半部分的 ` 有没有跟前面对应 如果不对应我们还要自己往下找
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
					// 最简单的情况 我们找到的前后 ` 个数相同 就是一个完整的内嵌code
					if (MatchResult[2].length() == MatchResult[4].length())
					{
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
						ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("code")));
						ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));
						ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/code")));
						continue;
					}
					// 前后个数不相同 我们只处理 前后个数大于后面的情况
					else if (MatchResult[2].length() > MatchResult[4].length())
					{
						// 得到前面 ` 的个数 来构造新的正则表达式 向后查找 等个数 `
						std::regex InlineCodeTailRegex("([^`]+)(`{" + std::to_string(MatchResult[2].length()) + "})(.*)");
						if (std::regex_match(MatchResult[5].first, (*i)->Text().end(), InlineTailResult, InlineCodeTailRegex))
						{
							// 找到了
							strIter = InlineTailResult[3].first;
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[1])));
							ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("code")));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[3])));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(MatchResult[4])));
							ProcessedList.push_back(CTokenPtr(new Token::CRawText(InlineTailResult[1])));
							ProcessedList.push_back(CTokenPtr(new Token::CHtmlTag("/code")));
							continue;
						}
						// 没有找到匹配的	
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