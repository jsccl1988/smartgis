// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/product/proj/shell/map_project.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMMapProjectApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMMapProjectApp::CSmtAMMapProjectApp() = default;

CSmtAMMapProjectApp theApp;

BOOL CSmtAMMapProjectApp::InitInstance() {
  CWinApp::InitInstance();
  bind_plugin_dll_resources(this);
  return TRUE;
}
