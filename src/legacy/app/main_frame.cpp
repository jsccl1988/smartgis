// main_frame.cpp : CMainFrame implementation
//

#include "legacy/app/stdafx.h"
#include "legacy/app/smart_gis.h"

#include "legacy/app/main_frame.h"

#include "legacy/ui/gui/edit_config_dock_bar.h"
#include "legacy/ui/gui/config_dock_bar.h"

// defs.h unused by this TU
#include "base/core/log.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

#define IDD_PRAPROSETTING_DOCBAR		1000

IMPLEMENT_DYNAMIC(CMainFrame, CMainWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMainWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_2003, ID_VIEW_APPLOOK_2007_1, OnAppLook)
	ON_REGISTERED_MESSAGE(BCGM_ON_GET_TAB_TOOLTIP, OnGetTabToolTip)
	ON_COMMAND(ID_WND_MAPEDIT, &CMainFrame::OnWndMapedit)
	ON_COMMAND(ID_WND_MAPDATA, &CMainFrame::OnWndMapdata)
	ON_COMMAND(ID_WND_3D, &CMainFrame::OnWnd3d)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // 状态行指示器
	ID_INDICATOR_XY,
	ID_INDICATOR_LB,
};


// CMainFrame ctor/dtor

CMainFrame::CMainFrame()
{
	// TODO: 在此添加成员初始化代码
	m_bAutoMenuEnable = false;

	m_pDSCatalog		  = NULL;
	m_pMapDocCatalog	  = NULL;
	m_p3DObjCatalog		  = NULL;
	
	m_nAppLook = theApp.GetInt (_T("ApplicationLook"), ID_VIEW_APPLOOK_2003);
}

CMainFrame::~CMainFrame()
{

}


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMainWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	OnAppLook (m_nAppLook);

	CMFCToolBar::EnableQuickCustomization ();

	UpdateMDITabs (FALSE);

	DWORD dwBCGStyle = CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE;
	
	//if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
	//	| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
	//	!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	//{
	//	TRACE0("未能创建工具栏\n");
	//	return -1;      // 未能创建
	//}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("未能创建状态栏\n");
		return -1;      // 未能创建
	}

	if (!m_wndMenuBar.Create (this)){
		TRACE0("Failed to create menubar\n");
		return -1; // fail to create
	}

	m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() |CBRS_TOOLTIPS | CBRS_FLYBY |CBRS_SIZE_DYNAMIC);

	if (!m_wndCatalogDocBar.Create (_T("Catalog"), this, CRect (0, 0, 250, 250),
		TRUE, ID_DOCB_LEFT,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI,
		CBRS_BCGP_OUTLOOK_TABS,
		dwBCGStyle))
	{
		TRACE0("Failed to create Workspace bar\n");
		return FALSE;      // fail to create
	}

	if (!m_wndAMBoxMgrDocBar.Create (_T("功能包管理器"), this, CRect (0, 0, 250, 250),
		ID_DOCB_RIGTH,
		WS_CHILD | WS_VISIBLE | CBRS_LEFT|WS_CLIPSIBLINGS | WS_CLIPCHILDREN |CBRS_FLOAT_MULTI,
		dwBCGStyle))
	{
		TRACE0("Failed to create Workspace bar\n");
		return FALSE;      // fail to create
	}

	// TODO: 如果不需要工具栏可停靠，则删除这三行
	EnableDocking(CBRS_ALIGN_ANY);
	//m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndMenuBar.EnableDocking (CBRS_ALIGN_ANY);
	m_wndCatalogDocBar.EnableDocking (CBRS_ALIGN_ANY);
	m_wndAMBoxMgrDocBar.EnableDocking (CBRS_ALIGN_ANY);
	
	//DockPane(&m_wndToolBar);
	DockPane(&m_wndMenuBar);
	DockPane(&m_wndCatalogDocBar, AFX_IDW_DOCKBAR_LEFT);
	DockPane(&m_wndAMBoxMgrDocBar, AFX_IDW_DOCKBAR_RIGHT);
	
	InitStatusBar();

	if (!InitCatalogDockBar())
		return -1;

	if (!InitAMBoxMgrDockBar())
		return -1;

	RecalcLayout ();

	return 0;
}

void CMainFrame::OnDestroy()
{
	CMainWnd::OnDestroy();

	// TODO: 在此处添加消息处理程序代码
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CMainWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式

	return TRUE;
}


// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMainWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMainWnd::Dump(dc);
}

#endif //_DEBUG


// CMainFrame message handlers

void CMainFrame::InitStatusBar()
{
	RECT rect;
	GetClientRect(&rect);
	CClientDC dc(this);
	TEXTMETRIC tm;
	short charwidth,width,MinSepWid;

	//得字符的平均宽度
	dc.GetTextMetrics(&tm);
	charwidth=(short)tm.tmAveCharWidth;

	//设置第0项的最小宽度
	MinSepWid = 36*charwidth;
	width=MinSepWid;
	m_wndStatusBar.SetPaneInfo(0,ID_SEPARATOR,SBPS_NORMAL,width/1);
	m_wndStatusBar.SetPaneInfo(1,ID_INDICATOR_XY,SBPS_NORMAL,width/1);
	m_wndStatusBar.SetPaneInfo(2,ID_INDICATOR_LB,SBPS_NORMAL,width/1.4);
}

void CMainFrame::SetStatusBarString(UINT index,CString str)
{
	m_wndStatusBar.SetPaneText(index,str);
}

void CMainFrame::OnAppLook(UINT id)
{
	CWaitCursor wait;

	switch (id)
	{
	case ID_VIEW_APPLOOK_2003:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS (CMFCVisualManagerOffice2003));
		theApp.m_Options.m_nTabsStyle = CBCGPTabWnd::STYLE_3D_VS2005;
		break;
	
	case ID_VIEW_APPLOOK_2007_1:
			CMFCVisualManagerOffice2007::SetStyle (CMFCVisualManagerOffice2007::Office2007_LunaBlue);
			CMFCVisualManager::SetDefaultManager (RUNTIME_CLASS (CMFCVisualManagerOffice2007));
		break;

	case ID_VIEW_APPLOOK_2007_2:
			CMFCVisualManagerOffice2007::SetStyle (CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
			CMFCVisualManager::SetDefaultManager (RUNTIME_CLASS (CMFCVisualManagerOffice2007));
		break;

	case ID_VIEW_APPLOOK_2007_3:
			CMFCVisualManagerOffice2007::SetStyle (CMFCVisualManagerOffice2007::Office2007_Silver);
			CMFCVisualManager::SetDefaultManager (RUNTIME_CLASS (CMFCVisualManagerOffice2007));
		break;

	case ID_VIEW_APPLOOK_2007_4:
			CMFCVisualManagerOffice2007::SetStyle (CMFCVisualManagerOffice2007::Office2007_Aqua);
			CMFCVisualManager::SetDefaultManager (RUNTIME_CLASS (CMFCVisualManagerOffice2007));
		break;
	}

	RecalcLayout ();

	RedrawWindow (NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW |  RDW_FRAME | RDW_ERASE);

	theApp.WriteInt (_T("ApplicationLook"), m_nAppLook);
}

void CMainFrame::UpdateMDITabs (BOOL bResetMDIChild)
{
	CBCGPMDITabParams params;

	switch (theApp.m_Options.m_nMDITabsType)
	{
	case CMDITabOptions::None:
		{
			BOOL bCascadeMDIChild = FALSE;

			if (IsMDITabbedGroup ())
			{
				EnableMDITabbedGroups (FALSE, params);
				bCascadeMDIChild = TRUE;
			}
			else if (AreMDITabs ())
			{
				EnableMDITabs (FALSE);
				bCascadeMDIChild = TRUE;
			}

			if (bCascadeMDIChild)
			{
				HWND hwndActive = (HWND) m_wndClientArea.SendMessage (WM_MDIGETACTIVE);
				m_wndClientArea.SendMessage (WM_MDICASCADE);
				::BringWindowToTop (hwndActive);
			}
		}
		break;

	case CMDITabOptions::MDITabsStandard:
		EnableMDITabs (TRUE,
			theApp.m_Options.m_bMDITabsIcons,
			theApp.m_Options.m_bTabsOnTop ? CBCGPTabWnd::LOCATION_TOP : CBCGPTabWnd::LOCATION_BOTTOM, 
			theApp.m_Options.m_bMaximizeMDIChild,
			theApp.m_Options.m_nTabsStyle);

		GetMDITabs().EnableAutoColor (theApp.m_Options.m_bTabsAutoColor);
		GetMDITabs().EnableTabDocumentsMenu (theApp.m_Options.m_bMDITabsDocMenu);
		GetMDITabs().EnableTabSwap (theApp.m_Options.m_bDragMDITabs);
		GetMDITabs().SetTabBorderSize (theApp.m_Options.m_nMDITabsBorderSize);
		GetMDITabs().SetFlatFrame (theApp.m_Options.m_bFlatFrame);
		GetMDITabs().EnableCustomToolTips (theApp.m_Options.m_bCustomTooltips);
		GetMDITabs().EnableCustomToolTips (theApp.m_Options.m_bCustomTooltips);
		GetMDITabs().EnableActiveTabCloseButton (theApp.m_Options.m_bActiveTabCloseButton);
		break;

	case CMDITabOptions::MDITabbedGroups:
		params.m_tabLocation = theApp.m_Options.m_bTabsOnTop ? CBCGPTabWnd::LOCATION_TOP : CBCGPTabWnd::LOCATION_BOTTOM;
		params.m_style = theApp.m_Options.m_nTabsStyle;
		params.m_bTabCloseButton = !theApp.m_Options.m_bActiveTabCloseButton;
		params.m_bActiveTabCloseButton = theApp.m_Options.m_bActiveTabCloseButton;
		params.m_bAutoColor = theApp.m_Options.m_bTabsAutoColor;
		params.m_bDocumentMenu = theApp.m_Options.m_bMDITabsDocMenu;
		params.m_bEnableTabSwap = theApp.m_Options.m_bDragMDITabs;
		params.m_nTabBorderSize = theApp.m_Options.m_nMDITabsBorderSize;
		params.m_bTabIcons = theApp.m_Options.m_bMDITabsIcons;
		params.m_bFlatFrame = theApp.m_Options.m_bFlatFrame;
		params.m_bTabCustomTooltips = theApp.m_Options.m_bCustomTooltips;

		EnableMDITabbedGroups (TRUE, params);
		break;
	}


	// Some "Windows..." commands are non-relevant when all MDI child windows
	// are always maximized:
	CList<UINT, UINT> lstCommands;

	if (!theApp.m_Options.IsMDITabsDisabled () && theApp.m_Options.m_bMaximizeMDIChild)
	{
		lstCommands.AddTail (ID_WINDOW_CASCADE);
		lstCommands.AddTail (ID_WINDOW_TILE_HORZ);
		lstCommands.AddTail (ID_WINDOW_ARRANGE);
	}

	CBCGPToolBar::SetNonPermittedCommands (lstCommands);

	if (bResetMDIChild)
	{
		// Adjust MDI child windows:
		BOOL bMaximize =	theApp.m_Options.m_bMaximizeMDIChild && 
			!theApp.m_Options.IsMDITabsDisabled ();

		HWND hwndT = ::GetWindow(m_hWndMDIClient, GW_CHILD);
		while (hwndT != NULL)
		{
			CBCGPMDIChildWnd* pFrame = DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, 
				CWnd::FromHandle (hwndT));
			if (pFrame != NULL)
			{
				ASSERT_VALID (pFrame);

				if (bMaximize)
				{
					pFrame->ModifyStyle (WS_SYSMENU, 0);
				}
				else
				{
					pFrame->ModifyStyle (0, WS_SYSMENU);
					pFrame->ShowWindow (SW_RESTORE);

					CRect rectFrame;
					pFrame->GetWindowRect (rectFrame);

					pFrame->SetWindowPos (NULL,
						-1, -1,
						rectFrame.Width () + 1, rectFrame.Height (),
						SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
					pFrame->SetWindowPos (NULL,
						-1, -1,
						rectFrame.Width (), rectFrame.Height (),
						SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
				}
			}

			hwndT=::GetWindow(hwndT,GW_HWNDNEXT);
		}

		if (bMaximize)
		{
			m_wndMenuBar.SetMaximizeMode (FALSE);
		}
		else
		{
			HWND hwndActive = (HWND) m_wndClientArea.SendMessage (WM_MDIGETACTIVE);
			m_wndClientArea.PostMessage (WM_MDICASCADE);
			::BringWindowToTop (hwndActive);
		}
	}

	CBCGPMDIFrameWnd::m_bDisableSetRedraw = theApp.m_Options.m_bDisableMDIChildRedraw;

	RecalcLayout ();
	RedrawWindow (NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void CMainFrame::OnWndMapedit()
{
	// TODO: 在此添加命令处理程序代码
	CMDIChildWnd *pActiveChild = (CMDIChildWnd *)GetActiveFrame();
	CDocument	 *pDoc = NULL;
	//
	if (NULL == pActiveChild ||NULL == (pDoc = pActiveChild->GetActiveFrame()->GetActiveDocument()))
	{
		return;
	}

	CDocTemplate *pTempate = theApp.GetEditViewDocTemplate();

	if (NULL != pTempate)
	{
		CFrameWnd *pFrame = pTempate->CreateNewFrame(pDoc,pActiveChild);
		if (NULL != pFrame)
		{
			pTempate->InitialUpdateFrame(pFrame,pDoc);
		}
	}
}

void CMainFrame::OnWndMapdata()
{
	// TODO: 在此添加命令处理程序代码
	CMDIChildWnd *pActiveChild = (CMDIChildWnd *)GetActiveFrame();
	CDocument	 *pDoc = NULL;
	//
	if (NULL == pActiveChild ||NULL == (pDoc = pActiveChild->GetActiveFrame()->GetActiveDocument()))
	{
		return;
	}

	CDocTemplate *pTempate = theApp.GetDataViewDocTemplate();

	if (NULL != pTempate)
	{
		CFrameWnd *pFrame = pTempate->CreateNewFrame(pDoc,pActiveChild);
		if (NULL != pFrame)
		{
			pTempate->InitialUpdateFrame(pFrame,pDoc);
		}
	}
}

void CMainFrame::OnWnd3d()
{
	// TODO: 在此添加命令处理程序代码
	CMDIChildWnd *pActiveChild = (CMDIChildWnd *)GetActiveFrame();
	CDocument	 *pDoc = NULL;
	//
	if (NULL == pActiveChild ||NULL == (pDoc = pActiveChild->GetActiveFrame()->GetActiveDocument()))
	{
		return;
	}

	CDocTemplate *pTempate = theApp.Get3DViewDocTemplate();

	if (NULL != pTempate)
	{
		CFrameWnd *pFrame = pTempate->CreateNewFrame(pDoc,pActiveChild);
		if (NULL != pFrame)
		{
			pTempate->InitialUpdateFrame(pFrame,pDoc);
		}
	}
}


CBCGPMDIChildWnd* CMainFrame::CreateDocumentWindow (LPCTSTR lpcszDocName)
{
	if (lpcszDocName != NULL && lpcszDocName [0] != '\0')
	{
		CDocument* pDoc = AfxGetApp()->OpenDocumentFile (lpcszDocName);

		if (pDoc != NULL)
		{
			POSITION pos = pDoc->GetFirstViewPosition();

			if (pos != NULL)
			{
				CView* pView = pDoc->GetNextView (pos);
				return DYNAMIC_DOWNCAST (CBCGPMDIChildWnd, pView->GetParent ());
			}   
		}
	}
	return NULL;
}

LRESULT CMainFrame::OnGetTabToolTip(WPARAM /*wp*/, LPARAM lp)
{
	CBCGPTabToolTipInfo* pInfo = (CBCGPTabToolTipInfo*) lp;
	ASSERT (pInfo != NULL);
	ASSERT_VALID (pInfo->m_pTabWnd);

	if (!pInfo->m_pTabWnd->IsMDITab ())
	{
		return 0;
	}
	pInfo->m_strText.Format (_T("Tab #%d Custom Tooltip"), pInfo->m_nTabIndex + 1);
	return 0;
}

bool CMainFrame::InitCatalogDockBar(void)
{
	if (!InitMapDocCatalog())
		return false;

	if (!Init3DObjCatalog())
		return false;

	if (!InitDSCatalog())
		return false;

	return true;
}

bool CMainFrame::InitAMBoxMgrDockBar(void)
{
	//edit config
	EditConfigDockBar *pEditCfgDockBar = new EditConfigDockBar();
	pEditCfgDockBar->Create (_T("设置"), m_wndAMBoxMgrDocBar.GetOnerWnd(), CRect (0, 0, 300, 300),
		TRUE, IDD_PRAPROSETTING_DOCBAR,WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_wndAMBoxMgrDocBar.AddWnd(pEditCfgDockBar,"编辑参数");

	//sys config
	SysConfigDockBar *pSysCfgDockBar = new SysConfigDockBar();
	pSysCfgDockBar->Create (_T("设置"), m_wndAMBoxMgrDocBar.GetOnerWnd(), CRect (0, 0, 300, 300),
		TRUE, IDD_PRAPROSETTING_DOCBAR,WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_wndAMBoxMgrDocBar.AddWnd(pSysCfgDockBar,"系统参数");


	return m_wndAMBoxMgrDocBar.UpdateAMBoxs();
}

//////////////////////////////////////////////////////////////////////////
bool CMainFrame::InitMapDocCatalog(void)
{
	m_pMapDocCatalog = new SmtMapDocXCatalog();
	if(!m_pMapDocCatalog->Create(WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS , CRect(0,0,0,0),m_wndCatalogDocBar.GetOnerWnd(),1))
	{
		TRACE0("Failed to create maptree");
		return false;
	}

	m_pMapDocCatalog->ModifyStyleEx(0,WS_EX_CLIENTEDGE);
	m_pMapDocCatalog->UpdateMapTree();

	m_wndCatalogDocBar.AddWnd(m_pMapDocCatalog,"地图文档");

	return true;
}

bool CMainFrame::Init3DObjCatalog(void)
{
	m_p3DObjCatalog = new Smt3DObjXCatalog();
	if(!m_p3DObjCatalog->Create(WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS , CRect(0,0,0,0),m_wndCatalogDocBar.GetOnerWnd(),1))
	{
		TRACE0("Failed to create 3dobjtree");
		return false;
	}

	m_p3DObjCatalog->ModifyStyleEx(0,WS_EX_CLIENTEDGE);
	m_p3DObjCatalog->Update3DObjTree();

	m_wndCatalogDocBar.AddWnd(m_p3DObjCatalog,"三维对象");

	return true;
}

bool CMainFrame::InitDSCatalog(void)
{
	m_pDSCatalog = new SmtDSXCatalog();
	if(!m_pDSCatalog->Create(WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS , CRect(0,0,0,0),m_wndCatalogDocBar.GetOnerWnd(),1))
	{
		TRACE0("Failed to create dstree");
		return false;
	}

	m_pDSCatalog->ModifyStyleEx(0,WS_EX_CLIENTEDGE);
	m_pDSCatalog->UpdateCatalogTree();

	m_wndCatalogDocBar.AddWnd(m_pDSCatalog,"数据源服务");

	return true;
}
