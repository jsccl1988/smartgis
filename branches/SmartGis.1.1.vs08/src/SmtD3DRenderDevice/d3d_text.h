/*
File:    d3d_text.h 

Desc:    SmtD3DText, D3D text Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.11.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3D_TEXT_H
#define _D3D_TEXT_H

#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DText
	{
	public:
		SmtD3DText();
		virtual ~SmtD3DText(void);

		//create font 
		long					CreateFont(HDC hDC,const char *chType, int nHeight,  int nWidth, int nWeight,  bool bItalic,bool bUnderline, bool bStrike, ulong ulSize);
		HFONT					GetFont();
		//draw text
		HRESULT					DrawText(HDC hDC,float xpos,float ypos,float zpos,const char*); //3d pos
		HRESULT					DrawText(HDC hDC,float xscreen,float yscreen,const char*);          //screen pos

	protected:
		uint					m_TextTb[65536];
		HFONT					m_hFont;
		int						m_nHeight,m_nWidth,m_nWeight;
	};
}
#endif //_D3D_TEXT_H