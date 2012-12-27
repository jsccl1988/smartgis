#include "msvr_mapserverdevice111.h"
#include "smt_api.h"
#include "ximage.h"
#include "smt_logmanager.h"
#include "msvr_mapserver.h"
#include <iostream>

using namespace Smt_Core;

namespace Smt_MapServer
{
	long SmtMapServerDevice111::WFSGetFeature(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		char szTextBuf[MSVR_BUF_LENGTH_32K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_32K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_32K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WFSGetLayer(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		char szTextBuf[MSVR_BUF_LENGTH_32K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_32K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_32K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WFSGetCapabilities(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		char szTextBuf[MSVR_BUF_LENGTH_32K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_32K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_32K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}
}