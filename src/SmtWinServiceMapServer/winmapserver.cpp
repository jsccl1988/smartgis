#include "winmapserver.h"

#include "smt_logmanager.h"
#include "msvr_mapserver.h"

using namespace Smt_MapServer;

#include <strsafe.h>

SmtWinMapService::SmtWinMapService(const char *szWinServiceName,const char *szWinServiceLog):SmtWinService(szWinServiceName,szWinServiceLog)
{
	;
}

SmtWinMapService::~SmtWinMapService()
{

}

long SmtWinMapService::New(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog)
{
	if (strlen(szWinServiceName) == 0)
		return SMT_ERR_INVALID_PARAM;

	if (NULL == m_pWinService)
	{
		SmtWinService::m_pWinService = new SmtWinMapService(szWinServiceName,szWinServiceLog);
	}

	pWinService = SmtWinService::m_pWinService;

	return SMT_ERR_NONE;
}

long SmtWinMapService::Delete(SmtWinService *&pWinService)
{
	//SMT_SAFE_DELETE(pWinService);

	SmtWinService::Delete(pWinService);

	return SMT_ERR_NONE;
}

long SmtWinMapService::UserRun()
{
	if( FAILED(::CoInitialize(NULL)) ) 
		return SMT_ERR_FAILURE;

	SmtMapServer mapSvr;
	if (SMT_ERR_NONE == mapSvr.Init() &&
		SMT_ERR_NONE == mapSvr.Create())
	{
		while (m_pWinService->m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
		{
			mapSvr.Run();
		}

		mapSvr.Destroy();
	}

	::CoUninitialize();

	return SMT_ERR_NONE;
}

long SmtWinMapService::UserCtrl(uchar dwOpcode)
{
	return SMT_ERR_NONE;
}
