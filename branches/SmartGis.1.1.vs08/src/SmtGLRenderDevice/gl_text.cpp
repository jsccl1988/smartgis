#include "gl_text.h"

namespace Smt_3Drd
{
	SmtGLText::SmtGLText()
	{
		for(int i=0; i<65536; i++)
		{
			m_TextTb[i] = 0;
		}
	}

	SmtGLText::~SmtGLText()
	{
		::DeleteObject(m_hFont);
		for(int i=0; i<65536; i++)
		{
			if(m_TextTb[i] != 0)
			{
				glDeleteLists(m_TextTb[i], 1);
			}
		}
	}

	//print implement
	long SmtGLText::CreateFont(HDC hDC,const char *chType, int nHeight,  int nWidth, int nWeight,  bool bItalic,bool bUnderline, bool bStrike, ulong ulSize)
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

	HFONT   SmtGLText::GetFont()
	{
		return m_hFont;
	}

	long SmtGLText::DrawText(HDC hDC,float x, float y,float z,const char *str) 
	{
		if(str == NULL ) 
			return SMT_ERR_INVALID_PARAM;

		::SelectObject(hDC,m_hFont);

		glRasterPos3f(x, y,z);
		wchar_t txt2[400];
		int len = MultiByteToWideChar(CP_ACP, 0, str, (int)strlen(str), txt2, 400 );
		for(int i=0; i<len; i++)
		{
			wchar_t letter = txt2[i];
			if(letter == L'\n')
			{
				y += m_nHeight;
				glRasterPos3f(x, y+m_nHeight,z);
			}
			else
			{
				if(m_TextTb[letter] == 0)
				{
					m_TextTb[letter] = glGenLists(1);
					wglUseFontBitmapsW(hDC, letter, 1, m_TextTb[letter]);
				}
				glCallList(m_TextTb[letter]);
			}
		}

		return SMT_ERR_NONE;
	}

	HRESULT SmtGLText::DrawText(HDC hDC,float x, float y,const char *str) 
	{
		if(str == NULL ) 
			return SMT_ERR_INVALID_PARAM;
		/*
		int length;
		length = (int)strlen(str);
		glRasterPos2f(x,y);
		for (int m = 0 ; m < length; m++)
		{
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,str[m]);
		}
		*/

		::SelectObject(hDC,m_hFont);

		glRasterPos2f(x, y);
		wchar_t txt2[400];
		int len = MultiByteToWideChar(CP_ACP, 0, str, (int)strlen(str), txt2, 400 );
		for(int i=0; i<len; i++)
		{
			wchar_t letter = txt2[i];
			if(letter == L'\n')
			{
				y += m_nHeight;
				glRasterPos2f(x, y+m_nHeight);
			}
			else
			{
				if(m_TextTb[letter] == 0)
				{
					m_TextTb[letter] = glGenLists(1);
					wglUseFontBitmapsW(hDC, letter, 1, m_TextTb[letter]);
				}
				glCallList(m_TextTb[letter]);
			}
		}

		return SMT_ERR_NONE;
	}
}