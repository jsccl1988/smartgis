// xcatalog_core.cpp : MFC Feature Pack extension-DLL attach.

#include "stdafx.h"
#include "legacy/ui/xcatalog/xcatalog_core.h"

#include <afxdllx.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "legacy/ui/mfc_ex/mfc_ext_support.h"

static AFX_EXTENSION_MODULE SmtXCatalogCoreDLL = {NULL, NULL};

extern "C" int APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason,
                                LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(lpReserved);
  if (dwReason == DLL_PROCESS_ATTACH) {
    if (!AfxInitExtensionModule(SmtXCatalogCoreDLL, hInstance)) {
      return 0;
    }
    new CDynLinkLibrary(SmtXCatalogCoreDLL);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    AfxTermExtensionModule(SmtXCatalogCoreDLL);
  }
  return 1;
}
