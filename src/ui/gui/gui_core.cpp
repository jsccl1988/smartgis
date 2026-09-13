// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include <afxdllx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "ui/mfc_ex/mfc_ext_support.h"

static AFX_EXTENSION_MODULE SmtGuiCoreDLL = {NULL, NULL};

extern "C" int APIENTRY DllMain(HINSTANCE hInstance,
                                DWORD dwReason,
                                LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(lpReserved);

  if (dwReason == DLL_PROCESS_ATTACH) {
    if (!AfxInitExtensionModule(SmtGuiCoreDLL, hInstance)) {
      return 0;
    }
    new CDynLinkLibrary(SmtGuiCoreDLL);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    AfxTermExtensionModule(SmtGuiCoreDLL);
  }
  return 1;
}
