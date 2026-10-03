// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/shell/catalog/tab_pane.h"

#include "legacy/core/macros/macros.h"

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
  HTREEITEM hit = find_tree_match(tree, tree->GetRootItem(), filter);
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
