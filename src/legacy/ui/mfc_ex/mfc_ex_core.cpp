// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Single AFX extension-DLL attach for ui_legacy (gui/mfc_ex/xview/xcatalog/xambox/chart).

#include "stdafx.h"
#include <afxdllx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "legacy/ui/mfc_ex/mfc_ext_support.h"

namespace tool {
extern HINSTANCE g_hInstance;
}

static AFX_EXTENSION_MODULE SmtUiLegacyDLL = {NULL, NULL};

extern "C" int APIENTRY DllMain(HINSTANCE hInstance,
                                DWORD dwReason,
                                LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(lpReserved);

  if (dwReason == DLL_PROCESS_ATTACH) {
    tool::g_hInstance = hInstance;
    if (!AfxInitExtensionModule(SmtUiLegacyDLL, hInstance)) {
      return 0;
    }
    new CDynLinkLibrary(SmtUiLegacyDLL);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    AfxTermExtensionModule(SmtUiLegacyDLL);
  }
  return 1;
}
