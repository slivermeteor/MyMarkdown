#pragma once
#include "Markdown.h"
#include <sstream>
#include <tchar.h>

namespace Markdown
{
	// 虚基类
	class CToken
	{
	public:
		CToken()
		{}
		virtual ~CToken()
		{}

		virtual const std::string& Text() const { return std::string(); }
		virtual bool IsBlankLine() const { return false; }			// 是否是空行
		virtual bool CanContainMarkup() const { return false; }		// 是否可以内嵌标签
		virtual bool IsContainer() const { return false; }			// 是否是容器 
		virtual bool InhibitParagraphs() const { return false; }	// 是否单独成段落

		virtual void WriteAsHTML(std::ostream& Out) const = 0;		// 纯虚函数 - 每个派生类都必须实现
		virtual CTokenList ProcessSectorElements() { return CTokenList(); }	//  区段元素处理函数
		virtual void HeaderWrite(std::ostream& Out) const {};		// HTML 头标签写入
		virtual void TailWrite(std::ostream& Out) const {};			// HTML 闭合标签写入 

	};

	enum ListType
	{
		UnorderList = 1,
		OrderList,
		NoneList
	};

	enum NextLineListType
	{
		NextCurListItem = 1,
		ListEnd,
		NoneNextList
	};

	enum TranscodingFlags
	{
		SelectTransferAmps = 0x1,
		TransferAmps = 0x10,
		TransferCompar = 0x100,
		QuotesBlock = 0x1000
	};


	// Markdown 处理
	CTokenPtr ParseHeader(CTokenListCIter& i, CTokenListCIter e);
	CTokenPtr ParseHorizontalRule(CTokenListCIter& i, CTokenListCIter e);
	CTokenPtr ParseListBlock(CTokenListCIter& i, CTokenListCIter e, bool bInline = false);		
	CTokenPtr ParseBlockQuote(CTokenListCIter& i, CTokenListCIter e);
	CTokenPtr ParseCodeBlock(CTokenListCIter& i, CTokenListCIter e);

	std::string IsCodeBlockLine(Markdown::CTokenListCIter& i, Markdown::CTokenListCIter e);
}

namespace Markdown::Token
{
	// 第二基类 CTextHolder
	class CTextHolder : public CToken
	{
	public:
		CTextHolder(const std::string& Text, bool CanContainMarkup, ULONG TranscodingFlags);
		
		virtual const std::string& Text() const { return m_Text; }
		virtual bool CanContainMarkup() const { return m_bCanContainMarkup; }
		virtual void WriteAsHTML(std::ostream& Out) const;

	private:
		const std::string m_Text;
		const bool m_bCanContainMarkup;
		const ULONG m_TranscodingFlags;
	};

	//  CRawText - 原始Text类
	class CRawText : public CTextHolder
	{
	public:
		CRawText(const std::string Text, bool bCanContainMarkup = true);

		virtual CTokenList ProcessSectorElements();
		void _ProcessInterlink(std::string SourText, CTokenList& ProcessedList);
		void _ProcessEmphasis(CTokenList& SourList);
		void _ProcessInlineCode(CTokenList& SourList);
	};

	// CBlankLine 空行
	class CBlankLine:public CTextHolder
	{
		public:
			CBlankLine(const std::string& ActualText = std::string());

			virtual bool IsBlankLine() const;
	};

	// CHeader - #{1,6}, UnderlineHeader 标题
	class CHeader :public CTextHolder
	{
		public:
			CHeader(size_t Level, const std::string& Text);
			virtual bool InhibitParagraphs() const { return true; }
			
			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;


		private:
			size_t m_Level;	// 记录几级标题
	};

	// CodeBlock 
	class CCodeBlock : public CTextHolder
	{
	public:
		CCodeBlock(const std::string& Contents);

		virtual void WriteAsHTML(std::ostream& Out) const;
	};

	// CHtmlTag -  html标签
	class CHtmlTag :public CTextHolder
	{
		public:
			CHtmlTag(const std::string& Text);

			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;
	};
	
	// CHtmlLinkTag
	class CHtmlLinkTag : public CTextHolder
	{
	public:
		CHtmlLinkTag(const std::string& URL, std::string& Title = std::string());
	};
	
	// CHtmlImgTag
	class CHtmlImgTag : public CTextHolder
	{
	public:
		CHtmlImgTag(const std::string& SRC, const std::string& ALT, std::string& Title = std::string());
	};
	
}

namespace Markdown::Token
{
	// 第二基类 Cantainer 容器
	class CContainer : public CToken
	{
	public:
		CContainer(const CTokenList& Contents = CTokenList());
		const CTokenList& SubTokens() const;
		virtual const std::string& Text() const;

		virtual bool IsContainer() const { return true; }
		virtual void  WriteAsHTML(std::ostream& Out) const;

		virtual CTokenList ProcessSectorElements();

		void  AddSubTokens(CTokenList& Tokens);
		void  SwapSubTokens(CTokenList& Tokens);

	protected:
		CTokenList m_SubTokens;
		bool m_bParagraphMode;
	};

	// CListItem
	class CListItem : public CContainer
	{
		public:
			CListItem(const CTokenList& Contents);

			void InhibitParagraphs(bool Value);
			virtual bool InhibitParagraphs() const;

			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;


		private:
			bool m_InhibitParagraphs;
	};

	// CUnorderedList
	class CUnorderedList : public CContainer
	{
		public:
			CUnorderedList(const CTokenList& Contents, bool bAsParagraphMode = false);


			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;
	};

	// COrderedList
	class COrderedList : public CUnorderedList
	{
		public:
			COrderedList(const CTokenList& Contents, bool bAsParagraphMode = false);

			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;
	};

	// CBlockQuote
	class CBlockQuote : public CContainer
	{
		public:
			CBlockQuote(const CTokenList& Contents);

			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;
	};

	// CParagraph
	class CParagraph : public CContainer
	{
		public:
			CParagraph(const CTokenList& Contents);
			CParagraph(const CTokenPtr& ParagrapnText);

			virtual void HeaderWrite(std::ostream& Out) const;
			virtual void TailWrite(std::ostream& Out) const;
	};
}