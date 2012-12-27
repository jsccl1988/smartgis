
#include "stdafx.h"
#include "TabbedWndDockBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// TabbedWndDockBar

BEGIN_MESSAGE_MAP(TabbedWndDockBar, CBCGPDockingControlBar)
	//{{AFX_MSG_MAP(TabbedWndDockBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// TabbedWndDockBar construction/destruction

TabbedWndDockBar::TabbedWndDockBar()
{
	// TODO: add one-time construction code here

}

TabbedWndDockBar::~TabbedWndDockBar()
{
	vector<CWnd*>::iterator iter = m_vWndPtrs.begin();
	while(iter != m_vWndPtrs.end())
	{
		if (*iter)
		{
			(*iter)->DestroyWindow();
			SMT_SAFE_DELETE(*iter);
		}
		iter++;
	}

	m_vWndPtrs.clear();
}

/////////////////////////////////////////////////////////////////////////////
// TabbedWndDockBar message handlers

int TabbedWndDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	CRect rectDummy;
	rectDummy.SetRectEmpty ();

	// Create tabs window:
	if (!m_wndTabs.Create (CBCGPTabWnd::STYLE_3D, rectDummy, this, 1))
	{
		TRACE0("Failed to create workspace tab window\n");
		return -1;      // fail to create
	}

	//m_wndTabs.SetImageList (IDB_WORKSPACE, 16, RGB (255, 0, 255));
	return 0;
}

void TabbedWndDockBar::OnSize(UINT nType, int cx, int cy) 
{
	CBCGPDockingControlBar::OnSize(nType, cx, cy);

	// Tab control should cover a whole client area:
	m_wndTabs.SetWindowPos (NULL, -1, -1, cx, cy,
		SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

BOOL TabbedWndDockBar::AddWnd(CWnd *pWnd,CString strLabel)
{
	m_wndTabs.AddTab (pWnd, LPCTSTR(strLabel));
	m_vWndPtrs.push_back(pWnd);

	return TRUE;
}
void TabbedWndDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
	// TODO: 在此处添加消息处理程序代码
}
