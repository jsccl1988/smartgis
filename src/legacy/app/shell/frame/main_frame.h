// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "legacy/app/shell/catalog/catalog_pane.h"
#include "legacy/app/shell/dock/diagnostic.h"
#include "legacy/core/macros/macros.h"
#include "legacy/ui/catalog/ds/catalog_ds.h"
#include "legacy/ui/catalog/map/catalog_map_doc.h"
#include "legacy/ui/catalog/scene/catalog_3d_obj.h"
#include "legacy/ui/shell/ambox/outlook_bar.h"

using namespace ui;

#define CMainWnd CBCGPMDIFrameWnd

// Leftover MDI main frame — Catalog / AMBox / Diagnostic docks + chrome.
class CMainFrame : public CMainWnd {
  DECLARE_DYNAMIC(CMainFrame)
 public:
  CMainFrame();

  void UpdateMDITabs(BOOL bResetMDIChild);
  CBCGPMDIChildWnd* CreateDocumentWindow(LPCTSTR lpcszDocName);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

  virtual ~CMainFrame();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  CBCGPMenuBar m_wndMenuBar;
  CBCGPStatusBar m_wndStatusBar;
  CatalogTabDockPane m_wndCatalogDocBar;
  SmtAMBoxMgrDocBar m_wndAMBoxMgrDocBar;
  DiagnosticToolsDockBar m_wndDiagnosticTools;

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

  // After Feature Pack LoadState — restore ASCII AM Outlook captions.
  void refresh_am_outlook_captions();

 private:
  void InitStatusBar(void);
  bool InitCatalogDockBar(void);
  bool InitAMBoxMgrDockBar(void);
  bool init_diagnostic_tools_dock_bar();

  bool InitMapDocCatalog(void);
  bool Init3DObjCatalog(void);
  bool InitDSCatalog(void);

 protected:
  UINT m_nAppLook;

 private:
  void apply_views_like_chrome_font();

  MapDocXCatalog* m_pMapDocCatalog;
  Smt3DObjXCatalog* m_p3DObjCatalog;
  SmtDSXCatalog* m_pDSCatalog;
  CFont ui_font_;
};
