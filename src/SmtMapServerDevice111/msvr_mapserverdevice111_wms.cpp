#include "msvr_mapserverdevice111.h"
#include "smt_api.h"
#include "ximage.h"
#include "smt_logmanager.h"
#include <iostream>
#include "msvr_mapserver.h"
#include <time.h>
#include "msvr_mapservercapabilities111.h"

using namespace Smt_Core;
namespace Smt_MapServer
{
	long SmtMapServerDevice111::WMSGetMap(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;
		 
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;
		
		vector<string> vStrMapNames;
		long	lZoom = 0;
		fRect	rct;
		lRect	view;
		bool	bTransparent = false;
		double	dbfXPRes = 1.,dbfYPRes = 1.;
		string  strFormat = "";

		if (STR_Tokenize(m_pJobData->mapKey2Value[c_str_msvr_wms_layers],vStrMapNames,",") < 1)
			return SMT_ERR_NONE;

		view.lb.x = view.lb.y = 0;
		view.rt.x = atof(m_pJobData->mapKey2Value[c_str_msvr_wms_width].c_str());
		view.rt.y = atof(m_pJobData->mapKey2Value[c_str_msvr_wms_height].c_str());

		/*strFormat= m_pJobData->mapKey2Value[c_str_msvr_wms_format];
		m_pJobData->pMapService->SetImageCode();*/

		bTransparent = (stricmp("TRUE",m_pJobData->mapKey2Value[c_str_msvr_wms_transparent].c_str()) == 0);

		m_pJobData->pMapService->SetTransparent(bTransparent);

		if (4 == sscanf_s(m_pJobData->mapKey2Value[c_str_msvr_wms_bbox].c_str(),"%f,%f,%f,%f",&(rct.lb.x),&(rct.lb.y),&(rct.rt.x),&(rct.rt.y)))
		{
			char *pImageBuf = NULL;
			long  lImageBufSize = 0;
			
			pLog->LogMessage("thread:%d\tjobname:%s\tlayers:%s",m_pWorkThread->GetThreadID(),m_szJobName,m_pJobData->mapKey2Value[c_str_msvr_wms_layers].c_str());
			
			if (SMT_ERR_NONE == m_pJobData->pMapService->GetVectorImage(pImageBuf,lImageBufSize,rct,view/*lZoom*/,vStrMapNames) &&
				SMT_ERR_NONE == SendBuf(pImageBuf,lImageBufSize,MSVR_RSP_MIME))
			{
				return SMT_ERR_NONE;
			}
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WMSGetCapabilities(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		string strAppTempPath = GetAppTempPath();
		string strXML = strAppTempPath+"WMSGetCapabilities.xml";
		char *pXMLBuf = NULL;
		long lBufSize = 0;

		SmtWMSCapabilities111 wmsCapa(m_pJobData->pMapService);

		if (SMT_ERR_NONE == wmsCapa.Create() &&
			SMT_ERR_NONE == wmsCapa.Save(strXML.c_str()) &&
			SMT_ERR_NONE == wmsCapa.GetXML(pXMLBuf,lBufSize))
		{
			if (SMT_ERR_NONE == SendBuf(pXMLBuf,lBufSize,MSVR_RSP_TEXT))
			{
				;
			}
			
			wmsCapa.FreeXMLBuf(pXMLBuf);
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice111::WMSGetFeatureInfo(void)
	{
		if (NULL == m_pJobData->pMapService)
			return SMT_ERR_FAILURE;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		vector<string>	vLayerNames;
		vector<string>	vQLayerNames;
		string			strInfoFormat;
		int				nFeaCount = 0;
		int				nX = 0,nY = 0;
		lRect			view;
		fRect			rct;

		view.lb.x = view.lb.y = 0;
		rct.lb.x = rct.lb.y = 0;

		if (STR_Tokenize(m_pJobData->mapKey2Value[c_str_msvr_wms_layers],vLayerNames,",") < 1)
			return SMT_ERR_NONE;

		if (STR_Tokenize(m_pJobData->mapKey2Value[c_str_msvr_wms_query_layers],vQLayerNames,",") < 1)
			return SMT_ERR_NONE;

		strInfoFormat	= m_pJobData->mapKey2Value[c_str_msvr_wms_info_format];
		nFeaCount		= atoi(m_pJobData->mapKey2Value[c_str_msvr_wms_feature_count].c_str());
		nX				= atoi(m_pJobData->mapKey2Value[c_str_msvr_wms_x].c_str());
		nY				= atoi(m_pJobData->mapKey2Value[c_str_msvr_wms_y].c_str());
		
		view.rt.x		= atof(m_pJobData->mapKey2Value[c_str_msvr_wms_width].c_str());
		view.rt.y		= atof(m_pJobData->mapKey2Value[c_str_msvr_wms_height].c_str());

		if (4 == sscanf_s(m_pJobData->mapKey2Value[c_str_msvr_wms_bbox].c_str(),"%f,%f,%f,%f",&(rct.lb.x),&(rct.lb.y),&(rct.rt.x),&(rct.rt.y)))
		{
			char szTextBuf[MSVR_BUF_LENGTH_32K];
			memset(szTextBuf,0,MSVR_BUF_LENGTH_32K);
			
			if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_32K,MSVR_RSP_TEXT))
			{
				return SMT_ERR_NONE;
			}
		}

		return SMT_ERR_FAILURE;
	}
}