#include <windows.h>
#include "stdio.h"
#include "tchar.h"

#include "smt_logmanager.h"
#include "winmapserver.h"

TCHAR						g_szServiceName[] = _T("SmtMapServer");
TCHAR						g_szLogName[] = _T("SmtMapServer");

int APIENTRY WinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR     lpCmdLine,
					 int       nCmdShow)
{

	SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
	pLogMgr->SetDefaultLog("SmtDefault");

	SmtWinService *pWinService = NULL;
	SmtWinMapService::New(pWinService,g_szServiceName,g_szLogName);

	if (NULL != pWinService &&
		pWinService->Init())
	{
		if (stricmp(lpCmdLine, "-install") == 0)
		{
			pWinService->Install();
		}
		else if (stricmp(lpCmdLine, "-uninstall") == 0)
		{
			pWinService->Uninstall();
		}
		else if (stricmp(lpCmdLine, "-start") == 0)
		{
			pWinService->Start();
		}
		else if (stricmp(lpCmdLine, "-restart") == 0)
		{
			pWinService->Restart();
		}
		else if (stricmp(lpCmdLine, "-stop") == 0)
		{
			pWinService->Stop();
		}
		else
		{
			pWinService->StartCtrlDispatcher();
		}
	}

	SmtWinMapService::Delete(pWinService);
	SmtLogManager::DestoryInstance();

	return 0;
}