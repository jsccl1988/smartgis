// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/product/model3d/shell/model_3d_creater.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAM3DModelCreaterApp, CWinApp)
END_MESSAGE_MAP()

CSmtAM3DModelCreaterApp::CSmtAM3DModelCreaterApp() = default;

CSmtAM3DModelCreaterApp theApp;

BOOL CSmtAM3DModelCreaterApp::InitInstance() {
  CWinApp::InitInstance();
  bind_plugin_dll_resources(this);
  return TRUE;
}
