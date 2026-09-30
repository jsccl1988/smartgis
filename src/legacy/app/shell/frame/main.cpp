// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/shell/frame/main.h"

#include "legacy/app/shell/frame/app.h"
#include "legacy/ui/inspect/config_dock_bar.h"
#include "legacy/ui/inspect/edit_config_dock_bar.h"

// defs.h unused by this TU
#include "base/core/log.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// CatalogTabDockPane — Feature Pack tab host for Catalog trees

BEGIN_MESSAGE_MAP(CatalogTabDockPane, CBCGPDockingControlBar)
ON_WM_CREATE()
ON_WM_SIZE()
ON_WM_CONTEXTMENU()
ON_EN_CHANGE(CatalogTabDockPane::kFilterEditId, &CatalogTabDockPane::OnEnChangeFilter)
END_MESSAGE_MAP()

CatalogTabDockPane::CatalogTabDockPane() = default;

CatalogTabDockPane::~CatalogTabDockPane() {
  for (CWnd* wnd : m_vWndPtrs) {
    if (wnd) {
      wnd->DestroyWindow();
      SMT_SAFE_DELETE(wnd);
    }
  }
  m_vWndPtrs.clear();
}

int CatalogTabDockPane::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPDockingControlBar::OnCreate(lpCreateStruct) == -1) {
    return -1;
  }

  CRect rectDummy;
  rectDummy.SetRectEmpty();

  if (!m_filter_edit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                ES_AUTOHSCROLL,
                            rectDummy, this, kFilterEditId)) {
    TRACE0("Failed to create catalog filter edit\n");
    return -1;
  }
  m_filter_edit.SendMessage(EM_SETCUEBANNER, TRUE,
                            reinterpret_cast<LPARAM>(L"Filter..."));

  if (!m_wndTabs.Create(CBCGPTabWnd::STYLE_3D, rectDummy, this, 1)) {
    TRACE0("Failed to create workspace tab window\n");
    return -1;
  }
  m_wndTabs.ShowWindow(SW_SHOW);
  return 0;
}

void CatalogTabDockPane::layout_children(int cx, int cy) {
  const int filter_h = 22;
  if (m_filter_edit.GetSafeHwnd()) {
    m_filter_edit.SetWindowPos(NULL, 0, 0, cx, filter_h,
                               SWP_NOZORDER | SWP_NOACTIVATE);
  }
  if (m_wndTabs.GetSafeHwnd()) {
    m_wndTabs.SetWindowPos(NULL, 0, filter_h, cx, max(0, cy - filter_h),
                           SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

void CatalogTabDockPane::OnSize(UINT nType, int cx, int cy) {
  CBCGPDockingControlBar::OnSize(nType, cx, cy);
  layout_children(cx, cy);
}

HTREEITEM CatalogTabDockPane::find_tree_match(CTreeCtrl* tree, HTREEITEM item,
                                              const CString& filter_lower) {
  while (item) {
    CString text = tree->GetItemText(item);
    text.MakeLower();
    if (text.Find(filter_lower) >= 0) {
      return item;
    }
    HTREEITEM child = tree->GetChildItem(item);
    if (child) {
      HTREEITEM hit = find_tree_match(tree, child, filter_lower);
      if (hit) {
        return hit;
      }
    }
    item = tree->GetNextSiblingItem(item);
  }
  return NULL;
}

void CatalogTabDockPane::apply_tree_filter() {
  CString filter;
  m_filter_edit.GetWindowText(filter);
  filter.Trim();
  if (filter.IsEmpty()) {
    return;
  }
  filter.MakeLower();

  const int tab = m_wndTabs.GetActiveTab();
  if (tab < 0) {
    return;
  }
  CWnd* page = m_wndTabs.GetTabWnd(tab);
  CTreeCtrl* tree = DYNAMIC_DOWNCAST(CTreeCtrl, page);
  if (!tree) {
    return;
  }
  HTREEITEM hit =
      find_tree_match(tree, tree->GetRootItem(), filter);
  if (hit) {
    tree->SelectItem(hit);
    tree->EnsureVisible(hit);
  }
}

void CatalogTabDockPane::OnEnChangeFilter() { apply_tree_filter(); }

BOOL CatalogTabDockPane::add_wnd(CWnd* pWnd, CString strLabel) {
  if (pWnd == NULL || pWnd->GetSafeHwnd() == NULL) {
    return FALSE;
  }

  m_wndTabs.AddTab(pWnd, LPCTSTR(strLabel), (UINT)-1, FALSE);
  m_vWndPtrs.push_back(pWnd);
  pWnd->ShowWindow(SW_SHOW);
  m_wndTabs.RecalcLayout();
  m_wndTabs.RedrawWindow(
      NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
  return TRUE;
}

void CatalogTabDockPane::OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/) {}

IMPLEMENT_DYNAMIC(CMainFrame, CMainWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMainWnd)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_COMMAND_RANGE(ID_VIEW_APPLOOK_2003, ID_VIEW_APPLOOK_2007_1, OnAppLook)
ON_REGISTERED_MESSAGE(BCGM_ON_GET_TAB_TOOLTIP, OnGetTabToolTip)
ON_COMMAND(ID_WND_MAPEDIT, &CMainFrame::OnWndMapedit)
ON_COMMAND(ID_WND_MAPDATA, &CMainFrame::OnWndMapdata)
ON_COMMAND(ID_WND_3D, &CMainFrame::OnWnd3d)
ON_COMMAND(ID_VIEW_DIAGNOSTIC_TOOLS, &CMainFrame::OnViewDiagnosticTools)
END_MESSAGE_MAP()

static UINT indicators[] = {
    ID_SEPARATOR,
    ID_INDICATOR_XY,
    ID_INDICATOR_LB,
};


CMainFrame::CMainFrame() {
  m_bAutoMenuEnable = false;

  m_pDSCatalog = NULL;
  m_pMapDocCatalog = NULL;
  m_p3DObjCatalog = NULL;

  m_nAppLook = theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_2007_1);
}

CMainFrame::~CMainFrame() {}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CMainWnd::OnCreate(lpCreateStruct) == -1) return -1;

  OnAppLook(m_nAppLook);

  CMFCToolBar::EnableQuickCustomization();

  UpdateMDITabs(FALSE);

  DWORD dwBCGStyle = CBRS_BCGP_FLOAT | CBRS_BCGP_AUTOHIDE | CBRS_BCGP_RESIZE;

  // if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE |
  // CBRS_TOP 	| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC)
  //|| 	!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
  //{
  // }

  if (!m_wndStatusBar.Create(this) ||
      !m_wndStatusBar.SetIndicators(indicators,
                                    sizeof(indicators) / sizeof(UINT))) {
    TRACE0("未能创建状态栏\n");
    return -1;
  }

  if (!m_wndMenuBar.Create(this)) {
    TRACE0("Failed to create menubar\n");
    return -1;
  }

  m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() | CBRS_TOOLTIPS |
                            CBRS_FLYBY | CBRS_SIZE_DYNAMIC);

  if (!m_wndCatalogDocBar.Create(
          _T("Catalog"), this, CRect(0, 0, 250, 250), TRUE, ID_DOCB_LEFT,
          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
              CBRS_LEFT | CBRS_FLOAT_MULTI,
          CBRS_BCGP_OUTLOOK_TABS, dwBCGStyle)) {
    TRACE0("Failed to create Workspace bar\n");
    return FALSE;
  }

  if (!m_wndAMBoxMgrDocBar.Create(
          _T("功能包管理器"), this, CRect(0, 0, 250, 250), ID_DOCB_RIGTH,
          WS_CHILD | WS_VISIBLE | CBRS_LEFT | WS_CLIPSIBLINGS |
              WS_CLIPCHILDREN | CBRS_FLOAT_MULTI,
          dwBCGStyle)) {
    TRACE0("Failed to create Workspace bar\n");
    return FALSE;
  }

  // Bottom Diagnostic strip (Views DiagnosticToolsPanel parity). Never nest
  // CBCGPDockingControlBar panes inside AMBox — that crashes Feature Pack.
  if (!m_wndDiagnosticTools.Create(
          _T("Diagnostic"), this, CRect(0, 0, 200, 180), TRUE,
          ID_DOCB_DIAGNOSTIC,
          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
              CBRS_BOTTOM | CBRS_FLOAT_MULTI,
          CBRS_BCGP_REGULAR_TABS, dwBCGStyle)) {
    TRACE0("Failed to create Diagnostic tools bar\n");
    return -1;
  }

  EnableDocking(CBRS_ALIGN_ANY);
  // m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
  m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY);
  m_wndCatalogDocBar.EnableDocking(CBRS_ALIGN_ANY);
  m_wndAMBoxMgrDocBar.EnableDocking(CBRS_ALIGN_ANY);
  m_wndDiagnosticTools.EnableDocking(CBRS_ALIGN_ANY);

  // DockPane(&m_wndToolBar);
  DockPane(&m_wndMenuBar);
  DockPane(&m_wndCatalogDocBar, AFX_IDW_DOCKBAR_LEFT);
  DockPane(&m_wndAMBoxMgrDocBar, AFX_IDW_DOCKBAR_RIGHT);
  DockPane(&m_wndDiagnosticTools, AFX_IDW_DOCKBAR_BOTTOM);

  InitStatusBar();

  if (!InitCatalogDockBar()) return -1;

  if (!InitAMBoxMgrDockBar()) return -1;

  if (!InitDiagnosticToolsDockBar()) return -1;

  RecalcLayout();

  return 0;
}

void CMainFrame::OnDestroy() {
  CMainWnd::OnDestroy();

}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
  if (!CMainWnd::PreCreateWindow(cs)) return FALSE;

  return TRUE;
}


#ifdef _DEBUG
void CMainFrame::AssertValid() const { CMainWnd::AssertValid(); }

void CMainFrame::Dump(CDumpContext& dc) const { CMainWnd::Dump(dc); }

#endif  //_DEBUG


void CMainFrame::InitStatusBar() {
  RECT rect;
  GetClientRect(&rect);
  CClientDC dc(this);
  TEXTMETRIC tm;
  short charwidth, width, MinSepWid;

  dc.GetTextMetrics(&tm);
  charwidth = (short)tm.tmAveCharWidth;

  MinSepWid = 36 * charwidth;
  width = MinSepWid;
  m_wndStatusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_NORMAL, width / 1);
  m_wndStatusBar.SetPaneInfo(1, ID_INDICATOR_XY, SBPS_NORMAL, width / 1);
  m_wndStatusBar.SetPaneInfo(2, ID_INDICATOR_LB, SBPS_NORMAL, width / 1.4);
}

void CMainFrame::SetStatusBarString(UINT index, CString str) {
  m_wndStatusBar.SetPaneText(index, str);
}

void CMainFrame::OnAppLook(UINT id) {
  CWaitCursor wait;

  m_nAppLook = id;

  switch (id) {
    case ID_VIEW_APPLOOK_2003:
      CMFCVisualManager::SetDefaultManager(
          RUNTIME_CLASS(CMFCVisualManagerOffice2003));
      theApp.m_Options.m_nTabsStyle = CBCGPTabWnd::STYLE_3D_VS2005;
      break;

    case ID_VIEW_APPLOOK_2007_1:
      CMFCVisualManagerOffice2007::SetStyle(
          CMFCVisualManagerOffice2007::Office2007_LunaBlue);
      CMFCVisualManager::SetDefaultManager(
          RUNTIME_CLASS(CMFCVisualManagerOffice2007));
      break;

    case ID_VIEW_APPLOOK_2007_2:
      CMFCVisualManagerOffice2007::SetStyle(
          CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
      CMFCVisualManager::SetDefaultManager(
          RUNTIME_CLASS(CMFCVisualManagerOffice2007));
      break;

    case ID_VIEW_APPLOOK_2007_3:
      CMFCVisualManagerOffice2007::SetStyle(
          CMFCVisualManagerOffice2007::Office2007_Silver);
      CMFCVisualManager::SetDefaultManager(
          RUNTIME_CLASS(CMFCVisualManagerOffice2007));
      break;

    case ID_VIEW_APPLOOK_2007_4:
      CMFCVisualManagerOffice2007::SetStyle(
          CMFCVisualManagerOffice2007::Office2007_Aqua);
      CMFCVisualManager::SetDefaultManager(
          RUNTIME_CLASS(CMFCVisualManagerOffice2007));
      break;
  }

  RecalcLayout();

  RedrawWindow(
      NULL, NULL,
      RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);

  theApp.WriteInt(_T("ApplicationLook"), m_nAppLook);
}

void CMainFrame::UpdateMDITabs(BOOL bResetMDIChild) {
  CBCGPMDITabParams params;

  switch (theApp.m_Options.m_nMDITabsType) {
    case CMDITabOptions::None: {
      BOOL bCascadeMDIChild = FALSE;

      if (IsMDITabbedGroup()) {
        EnableMDITabbedGroups(FALSE, params);
        bCascadeMDIChild = TRUE;
      } else if (AreMDITabs()) {
        EnableMDITabs(FALSE);
        bCascadeMDIChild = TRUE;
      }

      if (bCascadeMDIChild) {
        HWND hwndActive = (HWND)m_wndClientArea.SendMessage(WM_MDIGETACTIVE);
        m_wndClientArea.SendMessage(WM_MDICASCADE);
        ::BringWindowToTop(hwndActive);
      }
    } break;

    case CMDITabOptions::MDITabsStandard:
      EnableMDITabs(
          TRUE, theApp.m_Options.m_bMDITabsIcons,
          theApp.m_Options.m_bTabsOnTop ? CBCGPTabWnd::LOCATION_TOP
                                        : CBCGPTabWnd::LOCATION_BOTTOM,
          theApp.m_Options.m_bMaximizeMDIChild, theApp.m_Options.m_nTabsStyle);

      GetMDITabs().EnableAutoColor(theApp.m_Options.m_bTabsAutoColor);
      GetMDITabs().EnableTabDocumentsMenu(theApp.m_Options.m_bMDITabsDocMenu);
      GetMDITabs().EnableTabSwap(theApp.m_Options.m_bDragMDITabs);
      GetMDITabs().SetTabBorderSize(theApp.m_Options.m_nMDITabsBorderSize);
      GetMDITabs().SetFlatFrame(theApp.m_Options.m_bFlatFrame);
      GetMDITabs().EnableCustomToolTips(theApp.m_Options.m_bCustomTooltips);
      GetMDITabs().EnableCustomToolTips(theApp.m_Options.m_bCustomTooltips);
      GetMDITabs().EnableActiveTabCloseButton(
          theApp.m_Options.m_bActiveTabCloseButton);
      break;

    case CMDITabOptions::MDITabbedGroups:
      params.m_tabLocation = theApp.m_Options.m_bTabsOnTop
                                 ? CBCGPTabWnd::LOCATION_TOP
                                 : CBCGPTabWnd::LOCATION_BOTTOM;
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

      EnableMDITabbedGroups(TRUE, params);
      break;
  }

  // Some "Windows..." commands are non-relevant when all MDI child windows
  // are always maximized:
  CList<UINT, UINT> lstCommands;

  if (!theApp.m_Options.IsMDITabsDisabled() &&
      theApp.m_Options.m_bMaximizeMDIChild) {
    lstCommands.AddTail(ID_WINDOW_CASCADE);
    lstCommands.AddTail(ID_WINDOW_TILE_HORZ);
    lstCommands.AddTail(ID_WINDOW_ARRANGE);
  }

  CBCGPToolBar::SetNonPermittedCommands(lstCommands);

  if (bResetMDIChild) {
    BOOL bMaximize = theApp.m_Options.m_bMaximizeMDIChild &&
                     !theApp.m_Options.IsMDITabsDisabled();

    HWND hwndT = ::GetWindow(m_hWndMDIClient, GW_CHILD);
    while (hwndT != NULL) {
      CBCGPMDIChildWnd* pFrame =
          DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, CWnd::FromHandle(hwndT));
      if (pFrame != NULL) {
        ASSERT_VALID(pFrame);

        if (bMaximize) {
          pFrame->ModifyStyle(WS_SYSMENU, 0);
        } else {
          pFrame->ModifyStyle(0, WS_SYSMENU);
          pFrame->ShowWindow(SW_RESTORE);

          CRect rectFrame;
          pFrame->GetWindowRect(rectFrame);

          pFrame->SetWindowPos(NULL, -1, -1, rectFrame.Width() + 1,
                               rectFrame.Height(),
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
          pFrame->SetWindowPos(NULL, -1, -1, rectFrame.Width(),
                               rectFrame.Height(),
                               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
        }
      }

      hwndT = ::GetWindow(hwndT, GW_HWNDNEXT);
    }

    if (bMaximize) {
      m_wndMenuBar.SetMaximizeMode(FALSE);
    } else {
      HWND hwndActive = (HWND)m_wndClientArea.SendMessage(WM_MDIGETACTIVE);
      m_wndClientArea.PostMessage(WM_MDICASCADE);
      ::BringWindowToTop(hwndActive);
    }
  }

  CBCGPMDIFrameWnd::m_bDisableSetRedraw =
      theApp.m_Options.m_bDisableMDIChildRedraw;

  RecalcLayout();
  RedrawWindow(NULL, NULL,
               RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void CMainFrame::OnWndMapedit() {
  theApp.open_mdi_view(theApp.GetEditViewDocTemplate());
}

void CMainFrame::OnWndMapdata() {
  theApp.open_mdi_view(theApp.GetDataViewDocTemplate());
}

void CMainFrame::OnWnd3d() {
  theApp.open_mdi_view(theApp.Get3DViewDocTemplate());
}

CBCGPMDIChildWnd* CMainFrame::CreateDocumentWindow(LPCTSTR lpcszDocName) {
  if (lpcszDocName != NULL && lpcszDocName[0] != '\0') {
    CDocument* pDoc = AfxGetApp()->OpenDocumentFile(lpcszDocName);

    if (pDoc != NULL) {
      POSITION pos = pDoc->GetFirstViewPosition();

      if (pos != NULL) {
        CView* pView = pDoc->GetNextView(pos);
        return DYNAMIC_DOWNCAST(CBCGPMDIChildWnd, pView->GetParent());
      }
    }
  }
  return NULL;
}

LRESULT CMainFrame::OnGetTabToolTip(WPARAM /*wp*/, LPARAM lp) {
  CBCGPTabToolTipInfo* pInfo = (CBCGPTabToolTipInfo*)lp;
  ASSERT(pInfo != NULL);
  ASSERT_VALID(pInfo->m_pTabWnd);

  if (!pInfo->m_pTabWnd->IsMDITab()) {
    return 0;
  }
  pInfo->m_strText.Format(_T("Tab #%d Custom Tooltip"), pInfo->m_nTabIndex + 1);
  return 0;
}

bool CMainFrame::InitCatalogDockBar(void) {
  if (!InitMapDocCatalog()) return false;

  if (!Init3DObjCatalog()) return false;

  if (!InitDSCatalog()) return false;

  return true;
}

bool CMainFrame::InitAMBoxMgrDockBar(void) {
  // Feature Pack requires a unique control-bar ID per CDockablePane.
  constexpr UINT kIdEditConfigDock = 2101;
  constexpr UINT kIdSysConfigDock = 2102;

  EditConfigDockBar* pEditCfgDockBar = new EditConfigDockBar();
  pEditCfgDockBar->Create(
      _T("设置"), m_wndAMBoxMgrDocBar.get_oner_wnd(), CRect(0, 0, 300, 300),
      TRUE, kIdEditConfigDock,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

  m_wndAMBoxMgrDocBar.add_wnd(pEditCfgDockBar, "编辑参数");

  SysConfigDockBar* pSysCfgDockBar = new SysConfigDockBar();
  pSysCfgDockBar->Create(
      _T("设置"), m_wndAMBoxMgrDocBar.get_oner_wnd(), CRect(0, 0, 300, 300),
      TRUE, kIdSysConfigDock,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

  m_wndAMBoxMgrDocBar.add_wnd(pSysCfgDockBar, "系统参数");

  return m_wndAMBoxMgrDocBar.UpdateAMBoxs();
}

bool CMainFrame::InitDiagnosticToolsDockBar(void) {
  // Panes are created in DiagnosticToolsDockBar::OnCreate (Console | RenderTrace).
  return ::IsWindow(m_wndDiagnosticTools.GetSafeHwnd()) != FALSE;
}

void CMainFrame::OnViewDiagnosticTools() {
  const BOOL show = !m_wndDiagnosticTools.IsVisible();
  ShowPane(&m_wndDiagnosticTools, show, FALSE, TRUE);
  RecalcLayout();
}

bool CMainFrame::InitMapDocCatalog(void) {
  m_pMapDocCatalog = new SmtMapDocXCatalog();
  if (!m_pMapDocCatalog->Create(
          WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
              TVS_LINESATROOT | TVS_SHOWSELALWAYS,
          CRect(0, 0, 0, 0), m_wndCatalogDocBar.get_oner_wnd(), 1)) {
    TRACE0("Failed to create maptree");
    return false;
  }

  m_pMapDocCatalog->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
  m_pMapDocCatalog->UpdateMapTree();

  m_wndCatalogDocBar.add_wnd(m_pMapDocCatalog, "地图文档");

  return true;
}

bool CMainFrame::Init3DObjCatalog(void) {
  m_p3DObjCatalog = new Smt3DObjXCatalog();
  if (!m_p3DObjCatalog->Create(
          WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
              TVS_LINESATROOT | TVS_SHOWSELALWAYS,
          CRect(0, 0, 0, 0), m_wndCatalogDocBar.get_oner_wnd(), 1)) {
    TRACE0("Failed to create 3dobjtree");
    return false;
  }

  m_p3DObjCatalog->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
  m_p3DObjCatalog->Update3DObjTree();

  m_wndCatalogDocBar.add_wnd(m_p3DObjCatalog, "三维对象");

  return true;
}

bool CMainFrame::InitDSCatalog(void) {
  m_pDSCatalog = new SmtDSXCatalog();
  if (!m_pDSCatalog->Create(
          WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
              TVS_LINESATROOT | TVS_SHOWSELALWAYS,
          CRect(0, 0, 0, 0), m_wndCatalogDocBar.get_oner_wnd(), 1)) {
    TRACE0("Failed to create dstree");
    return false;
  }

  m_pDSCatalog->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
  m_pDSCatalog->UpdateCatalogTree();

  m_wndCatalogDocBar.add_wnd(m_pDSCatalog, "数据源服务");

  return true;
}
