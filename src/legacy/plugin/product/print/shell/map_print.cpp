// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/product/print/shell/map_print.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMMapPrintApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMMapPrintApp::CSmtAMMapPrintApp() = default;

CSmtAMMapPrintApp theApp;

BOOL CSmtAMMapPrintApp::InitInstance() {
  CWinApp::InitInstance();
  bind_plugin_dll_resources(this);
  return TRUE;
}
