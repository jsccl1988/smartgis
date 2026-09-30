
#include "stdafx.h"
#include "legacy/plugin/product/model3d/shell/model_3d_creater.h"

#include "legacy/plugin/runtime/auxmodule/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//		}

// CSmtAM3DModelCreaterApp

BEGIN_MESSAGE_MAP(CSmtAM3DModelCreaterApp, CWinApp)
END_MESSAGE_MAP()

CSmtAM3DModelCreaterApp::CSmtAM3DModelCreaterApp() {
  bind_plugin_dll_resources(this);
}

CSmtAM3DModelCreaterApp theApp;

BOOL CSmtAM3DModelCreaterApp::InitInstance() {
  CWinApp::InitInstance();

  return TRUE;
}
