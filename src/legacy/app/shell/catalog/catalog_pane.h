// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_CATALOG_PANE_H_
#define LEGACY_APP_SHELL_CATALOG_PANE_H_

#pragma once

#include <vector>

// Catalog host: docking pane + Feature Pack tab control + filter edit
// (replaces the former exported tabbed-dock shim).
class CatalogTabDockPane : public CBCGPDockingControlBar {
 public:
  CatalogTabDockPane();
  ~CatalogTabDockPane() override;

  CBCGPTabWnd* get_oner_wnd() { return &m_wndTabs; }
  BOOL add_wnd(CWnd* pWnd, CString strLabel);

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
  afx_msg void OnEnChangeFilter();

  void layout_children(int cx, int cy);
  void apply_tree_filter();
  static HTREEITEM find_tree_match(CTreeCtrl* tree, HTREEITEM item,
                                   const CString& filter_lower);

  DECLARE_MESSAGE_MAP()

 protected:
  enum { kFilterEditId = 42 };
  CEdit m_filter_edit;
  CBCGPTabWnd m_wndTabs;
  std::vector<CWnd*> m_vWndPtrs;
};

#endif  // LEGACY_APP_SHELL_CATALOG_PANE_H_
