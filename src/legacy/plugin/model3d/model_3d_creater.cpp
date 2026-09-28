
#include "stdafx.h"
#include "legacy/plugin/model3d/model_3d_creater.h"

#include "legacy/plugin/mfc_module.h"

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
