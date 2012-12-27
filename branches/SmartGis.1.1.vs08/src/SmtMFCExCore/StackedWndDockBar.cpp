#include "StdAfx.h"
#include "StackedWndDockBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCatalogDockBar

BEGIN_MESSAGE_MAP(StackedWndDockBar, CBCGPOutlookBar)
	//{{AFX_MSG_MAP(CCatalogDockBar)
	//}}AFX_MSG_MAP
	//	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCatalogDockBar construction/destruction

StackedWndDockBar::StackedWndDockBar()
{
	// TODO: add one-time construction code here
	m_nToolBoxPage = -1;

}

StackedWndDockBar::~StackedWndDockBar()
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
// StackedWndDockBar message handlers
void StackedWndDockBar::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/)
{
	// TODO: 在此处添加消息处理程序代码
}

//////////////////////////////////////////////////////////////////////////
bool StackedWndDockBar::AddWnd(CWnd* pWnd,CString strTitle)
{
	if (pWnd == NULL)
	{
		return false;
	}

	m_vWndPtrs.push_back(pWnd);

	CBCGPOutlookWnd* pContainer = DYNAMIC_DOWNCAST (CBCGPOutlookWnd,GetUnderlinedWindow ());

	if (pContainer == NULL)
	{
		TRACE0("Cannot get outlook bar container\n");
		return false;
	}

	pContainer->AddControl(pWnd,strTitle,0, TRUE,CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE);
	pWnd->ShowWindow(SW_SHOW);

	return true;
}

