// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/product/dem/shell/dem_creater.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMDemCreaterApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMDemCreaterApp::CSmtAMDemCreaterApp() = default;

CSmtAMDemCreaterApp theApp;

BOOL CSmtAMDemCreaterApp::InitInstance() {
  CWinApp::InitInstance();
  bind_plugin_dll_resources(this);
  return TRUE;
}
