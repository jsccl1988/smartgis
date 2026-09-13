#ifndef _CATALOGS_DOCKBAR_H
#define _CATALOGS_DOCKBAR_H
#if defined(MFC_EX_EXPORTS)
#define MFC_EX_EXPORT __declspec(dllexport)
#else
#define MFC_EX_EXPORT __declspec(dllimport)
#endif


#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "base/core/core.h"

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


#if !defined(MFC_EX_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"mfc_exD.lib")
#       else
#          pragma comment(lib,"mfc_ex.lib")
#	    endif  
#endif

#endif //_CATALOGS_DOCKBAR_H
