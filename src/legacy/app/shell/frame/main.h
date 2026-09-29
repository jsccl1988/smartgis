// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/core/macros/macros.h"
#include "legacy/app/shell/dock/diagnostic.h"
#include "legacy/ui/shell/ambox/ambox_dock_bar.h"
#include "legacy/ui/catalog/catalog_3d_obj.h"
#include "legacy/ui/catalog/catalog_ds.h"
#include "legacy/ui/catalog/catalog_map_doc.h"

#include <vector>

using namespace ui;

#define CMainWnd CBCGPMDIFrameWnd

// Catalog host: CDockablePane + embedded Feature Pack tab control (replaces
// the former exported tabbed-dock shim). File-local to the leftover shell.
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

class CMainFrame : public CMainWnd {
  DECLARE_DYNAMIC(CMainFrame)
 public:
  CMainFrame();

 public:
  void UpdateMDITabs(BOOL bResetMDIChild);
  CBCGPMDIChildWnd* CreateDocumentWindow(LPCTSTR lpcszDocName);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

 public:
  virtual ~CMainFrame();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  CBCGPMenuBar m_wndMenuBar;
  //	CBCGPToolBar				m_wndToolBar;
  CBCGPStatusBar m_wndStatusBar;
  CatalogTabDockPane m_wndCatalogDocBar;
  SmtAMBoxMgrDocBar m_wndAMBoxMgrDocBar;
  DiagnosticToolsDockBar m_wndDiagnosticTools;

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();

  afx_msg void OnAppLook(UINT id);
  afx_msg LRESULT OnGetTabToolTip(WPARAM wp, LPARAM lp);
  afx_msg void OnWndMapedit();
  afx_msg void OnWndMapdata();
  afx_msg void OnWnd3d();
  afx_msg void OnViewDiagnosticTools();

  DECLARE_MESSAGE_MAP()

 public:
  void SetStatusBarString(UINT index, CString str);

 private:
  void InitStatusBar(void);
  bool InitCatalogDockBar(void);
  bool InitAMBoxMgrDockBar(void);
  bool InitDiagnosticToolsDockBar(void);

 private:
  bool InitMapDocCatalog(void);
  bool Init3DObjCatalog(void);
  bool InitDSCatalog(void);

 protected:
  UINT m_nAppLook;

 private:
  SmtMapDocXCatalog* m_pMapDocCatalog;
  Smt3DObjXCatalog* m_p3DObjCatalog;
  SmtDSXCatalog* m_pDSCatalog;
};
