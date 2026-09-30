
#include "stdafx.h"
#include "legacy/plugin/product/proj/shell/map_project.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//		}

// CSmtAMMapProjectApp

BEGIN_MESSAGE_MAP(CSmtAMMapProjectApp, CWinApp)
END_MESSAGE_MAP()

CSmtAMMapProjectApp::CSmtAMMapProjectApp() { bind_plugin_dll_resources(this); }

CSmtAMMapProjectApp theApp;

BOOL CSmtAMMapProjectApp::InitInstance() {
  CWinApp::InitInstance();

  return TRUE;
}
