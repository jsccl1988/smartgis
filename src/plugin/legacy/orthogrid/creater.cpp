// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "plugin/legacy/orthogrid/creater.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CSmtAMOrthogridApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMOrthogridApp::CSmtAMOrthogridApp() {}

CSmtAMOrthogridApp theApp;

BOOL CSmtAMOrthogridApp::InitInstance() {
  CWinApp::InitInstance();
  return TRUE;
}
