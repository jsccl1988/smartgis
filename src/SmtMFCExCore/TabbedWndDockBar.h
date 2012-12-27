#ifndef _CATALOGS_DOCKBAR_H
#define _CATALOGS_DOCKBAR_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "smt_core.h"

class AFX_EXT_CLASS TabbedWndDockBar : public CBCGPDockingControlBar
{
public:
	TabbedWndDockBar();
	virtual ~TabbedWndDockBar();

public:
	CBCGPTabWnd*			GetOnerWnd(void) {return &m_wndTabs;}
	BOOL					AddWnd(CWnd *pWnd,CString strLabel);

protected:
	afx_msg int				OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void			OnSize(UINT nType, int cx, int cy);
	afx_msg void			OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

	DECLARE_MESSAGE_MAP()

protected:
	CBCGPTabWnd				m_wndTabs;
	vector<CWnd*>			m_vWndPtrs;
};


#if !defined(Export_SmtMFCExCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMFCExCoreD.lib")
#       else
#          pragma comment(lib,"SmtMFCExCore.lib")
#	    endif  
#endif

#endif //_CATALOGS_DOCKBAR_H
