#include "msvr_mapserverdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_MapServer
{
	SmtMapServerDevice::SmtMapServerDevice(void):m_pJobData(NULL)
	{

	}

	SmtMapServerDevice::~SmtMapServerDevice(void)
	{

	}

	void SmtMapServerDevice::Run(void *pParam)		
	{
		m_pJobData = (SmtMSVRJobData*)pParam;

		if (NULL != m_pJobData)
			Process();
	}

	long SmtMapServerDevice::Process(void)
	{
		if (NULL == m_pJobData->pMapService)
			return Default();

		if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_service].c_str(), c_str_msvr_wms.c_str()) == 0 )
		{//wms
			return WMSDispatch();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_service].c_str(), c_str_msvr_wmts.c_str()) == 0 )
		{//wmts
			return WMTSDispatch();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_service].c_str(), c_str_msvr_wfs.c_str()) == 0 )
		{//wfs
			return WFSDispatch();
		}
		else
		{
			return Default();
		}

		return SMT_ERR_FAILURE;
	}

	//wms
	long SmtMapServerDevice::WMSDispatch(void)
	{
		if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wms_getmap.c_str()) == 0 )
		{//GetMap
			return WMSGetMap();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wms_getcapabilities.c_str()) == 0 )
		{//GetCapabilities
			return WMSGetCapabilities();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wms_getfeatureinfo.c_str()) == 0 )
		{//WMSGetFeatureInfo
			return WMSGetFeatureInfo();
		}
		else
		{
			return Default();
		}

		return SMT_ERR_FAILURE;
	}

	//wfs
	long SmtMapServerDevice::WFSDispatch(void)
	{
		if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wfs_getfeature.c_str()) == 0 )
		{//GetFeature
			return WFSGetFeature();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wfs_getlayer.c_str()) == 0 )
		{//GetLayer
			return WFSGetLayer();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wfs_getcapabilities.c_str()) == 0 )
		{//GetCapabilities
			return WFSGetCapabilities();
		}
		else
		{
			return Default();
		}

		return SMT_ERR_FAILURE;
	}

	//wmts
	long SmtMapServerDevice::WMTSDispatch(void)
	{
		if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wmts_gettile.c_str()) == 0 )
		{//GetTile
			return WMTSGetTile();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wmts_getcapabilities.c_str()) == 0 )
		{//GetCapabilities
			return WMTSGetCapabilities();
		}
		else if (stricmp(m_pJobData->mapKey2Value[c_str_msvr_request].c_str(), c_str_msvr_wmts_getfeatureinfo.c_str()) == 0 )
		{//GetFeatureInfo
			return WMTSGetFeatureInfo();
		}
		else
		{
			return Default();
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice::Default()
	{
		char szTextBuf[MSVR_BUF_LENGTH_32K];
		memset(szTextBuf,0,MSVR_BUF_LENGTH_32K);

		if (SMT_ERR_NONE == SendBuf(szTextBuf,MSVR_BUF_LENGTH_32K,MSVR_RSP_TEXT))
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtMapServerDevice::SendBuf(const char * pBuffer, long lBufferSize,long type)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		static char responseBuf[MSVR_BUF_LENGTH_32K];	
		memset(responseBuf,0,MSVR_BUF_LENGTH_32K);

		long	lDataBufSize = lBufferSize;
		const char 	*pDataBuf = pBuffer;
		long	lBlockSize = MSVR_BUF_LENGTH_32K - sizeof(char) - sizeof(long);
		long	lSendBufSize = 0;

		m_cslock.Lock();
		//
		while(lDataBufSize > 0)
		{
			lSendBufSize = (lDataBufSize > lBlockSize)?lBlockSize:lDataBufSize;
			char *pBuf = responseBuf;
			pBuf[0] = type;
			pBuf++;
			memcpy(pBuf,(&lSendBufSize),sizeof(long));
			pBuf += sizeof(long);
			if (lDataBufSize > lBlockSize)
			{
				memcpy(pBuf,pDataBuf,lBlockSize);
				pDataBuf += lBlockSize;
				lDataBufSize -= lBlockSize;
			}
			else
			{
				memcpy(pBuf,pDataBuf,lDataBufSize);
				pDataBuf += lDataBufSize;
				lDataBufSize = 0;
			}

			pLog->LogMessage("send trd id = %d",m_pWorkThread->GetThreadID());

			m_pJobData->pSktServer->SendTo(responseBuf,sizeof(responseBuf), m_pJobData->requestAdr);
		}

		memset(responseBuf,0,MSVR_BUF_LENGTH_32K);
		responseBuf[0] = MSVR_RSP_END;

		m_pJobData->pSktServer->SendTo(responseBuf,sizeof(responseBuf), m_pJobData->requestAdr);

		m_cslock.Unlock();

		return SMT_ERR_NONE;
	}
}