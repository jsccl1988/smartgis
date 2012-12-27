#include "d3d_text.h"

namespace Smt_3Drd
{
	SmtD3DText::SmtD3DText()
	{
		for(int i=0; i<65536; i++)
		{
			m_TextTb[i] = 0;
		}
	}

	SmtD3DText::~SmtD3DText()
	{
		::DeleteObject(m_hFont);
		for(int i=0; i<65536; i++)
		{
			if(m_TextTb[i] != 0)
			{
				;//glDeleteLists(m_TextTb[i], 1);
			}
		}
	}

	//print implement
	long SmtD3DText::CreateFont(HDC hDC,const char *chType, int nHeight,  int nWidth, int nWeight,  bool bItalic,bool bUnderline, bool bStrike, ulong ulSize)
	{
		if(nHeight == 0 && nWidth == 0)
		{
			m_nHeight = MulDiv(ulSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);
			//m_nWidth  = MulDiv(dwSize, GetDeviceCaps(hDC, LOGPIXELSX), 72);
		}
		else
		{
			m_nHeight = nHeight;
			m_nWidth  = nWidth;
		}
		
		m_nWeight = nWeight;

		if (stricmp(chType, "symbol") == 0)
		{
			m_hFont = ::CreateFont(m_nHeight,m_nWidth,0,0,m_nWeight,bItalic,bUnderline,bStrike,SYMBOL_CHARSET,
				OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
				FF_DONTCARE|DEFAULT_PITCH,chType);
		}
		else
		{
			m_hFont = ::CreateFont(m_nHeight,m_nWidth,0,0,m_nWeight,bItalic,bUnderline,bStrike,ANSI_CHARSET,
				OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
				FF_DONTCARE|DEFAULT_PITCH,chType);
		}


		if(m_hFont == NULL) 
			return  SMT_FALSE;

		return SMT_OK;
	}

	HFONT   SmtD3DText::GetFont()
	{
		return m_hFont;
	}

	long SmtD3DText::DrawText(HDC hDC,float x, float y,float z,const char *str) 
	{
		if(str == NULL ) 
			return SMT_ERR_INVALID_PARAM;

		return SMT_ERR_NONE;
	}

	HRESULT SmtD3DText::DrawText(HDC hDC,float x, float y,const char *str) 
	{
		if(str == NULL ) 
			return SMT_ERR_INVALID_PARAM;

		return SMT_ERR_NONE;
	}
}