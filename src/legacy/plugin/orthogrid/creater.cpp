// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/plugin/orthogrid/creater.h"

#include "legacy/plugin/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMOrthogridApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMOrthogridApp::CSmtAMOrthogridApp() {
  bind_plugin_dll_resources(this);
}

CSmtAMOrthogridApp theApp;

BOOL CSmtAMOrthogridApp::InitInstance() {
  CWinApp::InitInstance();
  return TRUE;
}
