
#include "stdafx.h"
#include "legacy/plugin/dem/dem_creater.h"

#include "legacy/plugin/mfc_module.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//		}

// CSmtAMDemCreaterApp

BEGIN_MESSAGE_MAP(CSmtAMDemCreaterApp, CWinApp)
END_MESSAGE_MAP()


CSmtAMDemCreaterApp::CSmtAMDemCreaterApp() {
  bind_plugin_dll_resources(this);
}


CSmtAMDemCreaterApp theApp;


BOOL CSmtAMDemCreaterApp::InitInstance() {
  CWinApp::InitInstance();

  return TRUE;
}
