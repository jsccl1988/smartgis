// xambox_core.cpp : MFC Feature Pack extension-DLL attach.

#include "ui/xambox/stdafx.h"
#include "ui/xambox/xambox_core.h"
#include <afxdllx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "ui/mfc_ex/mfc_ext_support.h"

static AFX_EXTENSION_MODULE SmtXAMBoxCoreDLL = {NULL, NULL};

extern "C" int APIENTRY DllMain(HINSTANCE hInstance,
                                DWORD dwReason,
                                LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(lpReserved);
  if (dwReason == DLL_PROCESS_ATTACH) {
    if (!AfxInitExtensionModule(SmtXAMBoxCoreDLL, hInstance)) {
      return 0;
    }
    new CDynLinkLibrary(SmtXAMBoxCoreDLL);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    AfxTermExtensionModule(SmtXAMBoxCoreDLL);
  }
  return 1;
}
