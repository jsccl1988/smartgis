// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/product/orthogrid/shell/creater.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMOrthogridApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMOrthogridApp::CSmtAMOrthogridApp() = default;

CSmtAMOrthogridApp theApp;

BOOL CSmtAMOrthogridApp::InitInstance() {
  CWinApp::InitInstance();
  bind_plugin_dll_resources(this);
  return TRUE;
}
