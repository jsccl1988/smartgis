// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#ifndef __AFXWIN_H__
#error "include stdafx.h before this file for PCH"
#endif

#include "legacy/app/core/bootstrap.h"
#include "legacy/app/resource.h"
#include "legacy/app/shell/frame/mdi_tabs.h"
#include "legacy/core/macros/macros.h"
#include "legacy/core/types/env.h"

using namespace base;
using namespace app;

// Leftover CWinAppEx host — MDI templates + chrome; GIS bootstrap is SmtApp.
class CSmartGisApp : public CWinAppEx, public SmtApp {
 public:
  CSmartGisApp();

  virtual BOOL InitInstance();
  virtual int ExitInstance();

  virtual void PreLoadState();

  afx_msg void OnAppAbout();
  DECLARE_MESSAGE_MAP()

 public:
  inline CMultiDocTemplate* GetEditViewDocTemplate() {
    return m_pEditViewDocTemplate;
  }
  inline CMultiDocTemplate* GetDataViewDocTemplate() {
    return m_pDataViewDocTemplate;
  }
  inline CMultiDocTemplate* Get3DViewDocTemplate() {
    return m_p3DViewDocTemplate;
  }

  // Window popup on the dynamic view menu (RC Window menu is replaced).
  void append_mdi_window_menu(HMENU menu);
  // Open another MDI view on the active document, or a new doc if none.
  BOOL open_mdi_view(CDocTemplate* tmpl);

  CView* GetActiveDocView(CRuntimeClass* pViewClass);
  CView* GetActiveView(void);
  CDocument* GetActiveDoc(void);

  CMDITabOptions m_Options;

 protected:
  void restore_mfc_module_state() override;

  CMultiDocTemplate* m_pEditViewDocTemplate;
  CMultiDocTemplate* m_pDataViewDocTemplate;
  CMultiDocTemplate* m_p3DViewDocTemplate;
};

extern CSmartGisApp theApp;
