
#include "stdafx.h"
#include "legacy/plugin/print/map_print.h"

#include "legacy/plugin/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//		}

// CSmtAMMapPrintApp

BEGIN_MESSAGE_MAP(CSmtAMMapPrintApp, CWinApp)
END_MESSAGE_MAP()


CSmtAMMapPrintApp::CSmtAMMapPrintApp() {
  bind_plugin_dll_resources(this);
}


CSmtAMMapPrintApp theApp;


BOOL CSmtAMMapPrintApp::InitInstance() {
  CWinApp::InitInstance();

  return TRUE;
}
