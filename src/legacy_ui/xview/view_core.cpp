// view_core.cpp : MFC Feature Pack extension-DLL attach.

#include "stdafx.h"
#include "legacy_ui/xview/view_core.h"
#include <afxdllx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "legacy_ui/mfc_ex/mfc_ext_support.h"

static AFX_EXTENSION_MODULE SmtViewCoreDLL = {NULL, NULL};

extern "C" int APIENTRY DllMain(HINSTANCE hInstance,
                                DWORD dwReason,
                                LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(lpReserved);
  if (dwReason == DLL_PROCESS_ATTACH) {
    if (!AfxInitExtensionModule(SmtViewCoreDLL, hInstance)) {
      return 0;
    }
    new CDynLinkLibrary(SmtViewCoreDLL);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    AfxTermExtensionModule(SmtViewCoreDLL);
  }
  return 1;
}
