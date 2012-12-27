#include "msvr_mapserverdevice111.h"
#include "smt_api.h"
#include "ximage.h"
#include "smt_logmanager.h"
#include <iostream>
#include "msvr_mapserver.h"
#include <time.h>

using namespace Smt_Core;

namespace Smt_MapServer
{
	long SmtMapServerDevice111::WMTSGetTile(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;
		 
		string strMapName;
		long   lZoom = 0;
		long   lCol = 0;
		long   lRow = 0;

		strMapName = m_pJobData->mapKey2Value[c_str_msvr_wms_layers];
		lZoom	 = atol(m_pJobData->mapKey2Value[c_str_msvr_wmts_gettile_level].c_str());
		lCol	 = atol(m_pJobData->mapKey2Value[c_str_msvr_wmts_gettile_col].c_str());
		lRow	 = atol(m_pJobData->mapKey2Value[c_str_msvr_wmts_gettile_row].c_str());

		//»»Ëãcol
		long lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope env;

		//¼ÆËãtile·¶Î§
		m_pJobData->pMapService->GetEnvelope(env);
		m_pJobData->pMapService->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow,env,lZoom);

		lRow = lMaxRow - lRow;

		char *pImageBuf = NULL;
		long  lImageBufSize = 0;
		
		pLog->LogMessage("%d\t%s\t%s",m_pWorkThread->GetThreadID(),m_szJobName,m_pJobData->mapKey2Value[c_str_msvr_wms_layers].c_str());

		if (SMT_ERR_NONE == m_pJobData->pMapService->GetTileImage(pImageBuf,lImageBufSize,lCol,lRow,lZoom)&&
			SMT_ERR_NONE == SendBuf(pImageBuf,lImageBufSize,MSVR_RSP_MIME))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WMTSGetCapabilities(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		char szTextBuf[MSVR_BUF_LENGTH_1K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_1K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_1K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WMTSGetFeatureInfo(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		char szTextBuf[MSVR_BUF_LENGTH_1K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_1K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_1K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}
}