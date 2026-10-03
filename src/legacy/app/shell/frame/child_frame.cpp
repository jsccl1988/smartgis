// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/shell/frame/child_frame.h"

#include "base/core/log.h"
#include "legacy/app/shell/frame/win_app.h"
#include "legacy/app/view/scene3d_view.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CChildFrame, CChildWnd)

BEGIN_MESSAGE_MAP(CChildFrame, CChildWnd)
ON_WM_CREATE()
ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

CChildFrame::CChildFrame() {}

CChildFrame::~CChildFrame() {}

BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs) {
  cs.style = (cs.style & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;

  if (!CChildWnd::PreCreateWindow(cs)) return FALSE;

  return TRUE;
}

#ifdef _DEBUG
void CChildFrame::AssertValid() const { CChildWnd::AssertValid(); }

void CChildFrame::Dump(CDumpContext& dc) const { CChildWnd::Dump(dc); }

#endif  //_DEBUG

void CChildFrame::ActivateFrame(int nCmdShow) {
  nCmdShow = SW_MAXIMIZE;
  CChildWnd::ActivateFrame(nCmdShow);
}

BOOL CChildFrame::OnCommand(WPARAM wParam, LPARAM lParam) {
  CView* pView = GetActiveView();
  if (pView) {
    pView->PostMessage(WM_COMMAND, wParam, lParam);
  }

  return CBCGPMDIChildWnd::OnCommand(wParam, lParam);
}

namespace {

// SmartGis uses MDITabbedGroups — GetMDITabs() returns the unused standard
// tab ctrl (not created). Touching it AVs when opening Scene3D. Walk groups
// first; fall back to AreMDITabs() standard ctrl only when enabled.
void set_mdi_child_tab_label(CFrameWnd* frame, const CString& title) {
  if (!frame || title.IsEmpty()) {
    return;
  }
  CMDIFrameWndEx* main =
      DYNAMIC_DOWNCAST(CMDIFrameWndEx, AfxGetMainWnd());
  if (!main) {
    return;
  }
  if (main->IsMDITabbedGroup()) {
    const CObList& groups = main->GetMDITabGroups();
    for (POSITION pos = groups.GetHeadPosition(); pos != nullptr;) {
      CMFCTabCtrl* tabs =
          DYNAMIC_DOWNCAST(CMFCTabCtrl, groups.GetNext(pos));
      if (!tabs || !::IsWindow(tabs->GetSafeHwnd())) {
        continue;
      }
      const int n = tabs->GetTabsNum();
      for (int i = 0; i < n; ++i) {
        if (tabs->GetTabWnd(i) == frame) {
          tabs->SetTabLabel(i, title);
          return;
        }
      }
    }
    return;
  }
  if (!main->AreMDITabs()) {
    return;
  }
  CMFCTabCtrl& tabs = main->GetMDITabs();
  if (!::IsWindow(tabs.GetSafeHwnd())) {
    return;
  }
  const int n = tabs.GetTabsNum();
  for (int i = 0; i < n; ++i) {
    if (tabs.GetTabWnd(i) == frame) {
      tabs.SetTabLabel(i, title);
      return;
    }
  }
}

}  // namespace

void CChildFrame::OnUpdateFrameTitle(BOOL bAddToTitle) {
  // Shared CSmartGisDoc titles all MDI children "EDIT1:N". Keep the frame /
  // tab text set by Smt3DXView (Scene3D) instead of rewriting from the doc.
  if (CView* view = GetActiveView()) {
    if (view->IsKindOf(RUNTIME_CLASS(CSmart3DView))) {
      CString title;
      GetWindowText(title);
      if (!title.IsEmpty() && title.Find(_T("Scene3D")) >= 0) {
        set_mdi_child_tab_label(this, title);
        return;
      }
    }
  }
  CChildWnd::OnUpdateFrameTitle(bAddToTitle);
}

int CChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CBCGPMDIChildWnd::OnCreate(lpCreateStruct) == -1) return -1;

  return 0;
}

void CChildFrame::OnSysCommand(UINT nID, LPARAM lParam) {
  if (nID == WM_CLOSE) return;

  CBCGPMDIChildWnd::OnSysCommand(nID, lParam);
}

BOOL CChildFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName,
                         DWORD dwStyle, const RECT& rect,
                         CMDIFrameWnd* pParentWnd, CCreateContext* pContext) {
  dwStyle = (dwStyle & (~WS_SYSMENU)) | WS_MAXIMIZEBOX;
  return CBCGPMDIChildWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect,
                                  pParentWnd, pContext);
}
