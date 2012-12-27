#ifndef _STACKEDWND_DOCKBAR_H
#define _STACKEDWND_DOCKBAR_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "smt_core.h"

class AFX_EXT_CLASS StackedWndDockBar : public CBCGPOutlookBar
{
public:
	StackedWndDockBar();
	virtual ~StackedWndDockBar();

public:
	bool					AddWnd(CWnd* pWnd,CString strTitle);
	CBCGPOutlookWnd*		GetOnerWnd(void) {return  DYNAMIC_DOWNCAST (CBCGPOutlookWnd,GetUnderlinedWindow ());}

protected:
	afx_msg void			OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

	DECLARE_MESSAGE_MAP()

protected:
	vector<CWnd*>			m_vWndPtrs;
	int						m_nToolBoxPage;
};

#if !defined(Export_SmtMFCExCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMFCExCoreD.lib")
#       else
#          pragma comment(lib,"SmtMFCExCore.lib")
#	    endif  
#endif


#endif //_STACKEDWND_DOCKBAR_H

