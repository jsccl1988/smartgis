#include "gl_3drenderdevice.h"
#include "gl_text.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	//print implement
	long SmtGLRenderDevice::CreateFont(const char *chType,int nHeight,  int nWidth,  int nWeight, bool bItalic,bool bUnderline, bool bStrike, ulong dwSize,uint &unID) 
	{
		HDC hDC = ::GetDC( m_hWnd );

		SmtGLText *pText = new SmtGLText();
		if(SMT_ERR_NONE != pText->CreateFont(hDC,chType,nHeight,nWidth,nWeight,bItalic,bUnderline,bStrike,dwSize))
			return SMT_ERR_FAILURE;

		::ReleaseDC(m_hWnd,hDC);

		m_vTextPtrs.push_back(pText);
		unID = m_vTextPtrs.size() - 1;

		return SMT_ERR_NONE;
	}
}