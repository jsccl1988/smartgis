#include "msvr_mapserver.h"
#include "smt_api.h"
#include "cata_mapmgr.h"
#include "gis_api.h"
#include "sde_datasourcemgr.h"
#include "smt_logmanager.h"
#include "cata_mapservicemgr.h"
#include "msvr_mapserverdevicemgr.h"
#include <iostream>

using namespace Smt_GIS;
using namespace Smt_SDEDevMgr;
using namespace Smt_XCatalog;
namespace Smt_MapServer
{
	SmtMapServer::SmtMapServer():m_pThreadPool(NULL)
		,m_unPort(9001)
	{

	}

	SmtMapServer::~SmtMapServer()
	{
		m_sServer.Close();
	
		Destroy();
	}

	long SmtMapServer::Init(uint unPort)
	{
		m_unPort = unPort;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_LOG_NAME);
		if (NULL == pLog)
			pLog = pLogMgr->CreateLog(MAPSEVER_LOG_NAME);
		
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		pLog = pLogMgr->GetLog(MAPSEVER_DEVICE_LOG_NAME);
		if (NULL == pLog)
			pLog = pLogMgr->CreateLog(MAPSEVER_DEVICE_LOG_NAME);
		
		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtMapServer::Create(void)
	{
		/////////////////////////////////////////////////////////////////////////
		//创建网络连接
		int nBufLen = 0;	//接收数据缓冲区大小
		int nOptlen = sizeof(nBufLen);
		int uiNewRcvBuf;

		if (SMT_ERR_NONE == this->m_sServer.Create(SOCK_DGRAM) &&
			SMT_ERR_NONE == m_sServer.GetSockOpt( SO_RCVBUF,(char*)&nBufLen, nOptlen,SOL_SOCKET) &&					//获取接收数据缓冲区大小
			(true,nBufLen = MSVR_BUF_LENGTH_64K) &&
			SMT_ERR_NONE == m_sServer.SetSockOpt( SO_RCVBUF,(char*)&nBufLen, nOptlen,SOL_SOCKET) &&					//设置接收数据缓冲区为原来的10倍
			SMT_ERR_NONE == m_sServer.GetSockOpt( SO_RCVBUF,(char*)&uiNewRcvBuf, nOptlen,SOL_SOCKET) &&
			uiNewRcvBuf == nBufLen )													//检查设置系统接收数据缓冲区是否成功
		{
			if (SMT_ERR_NONE != m_sServer.Bind(TransAddr("localhost", m_unPort)))
			{
				SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
				SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_LOG_NAME);

				pLog->LogMessage("port:%d is already used! ",m_unPort);

				return SMT_ERR_FAILURE;
			}
		}

		/////////////////////////////////////////////////////////////////////////
		//创建线程池
		m_pThreadPool = new SmtThreadPool(100,10,50,20);

		//////////////////////////////////////////////////////////////////////////
		//创建服务
		SmtMapServiceMgr * pMapServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
		if (pMapServiceMgr)
		{
			string	strMSVRCfg = GetAppPath() + "sys\\smartgis.msvr";
			pMapServiceMgr->OpenMSVRCfg(strMSVRCfg.c_str());
		}
		
		return SMT_ERR_NONE;
	}

	long SmtMapServer::Destroy(void)
	{
		SMT_SAFE_DELETE(m_pThreadPool);

		SmtMapServerDeviceMgr::DestoryInstance();
		SmtMapServiceMgr::DestoryInstance();
		SmtLogManager::DestoryInstance();

		return SMT_ERR_NONE;
	}

	long SmtMapServer::Run(void)
	{
		if (NULL == m_pThreadPool)
			return SMT_ERR_FAILURE;
		 
		SOCKADDR_IN from;
		u_int  port;
		string strAddr;
		char revBuf[MSVR_BUF_LENGTH_1K];
		char szJobName[SMT_JOBNAME_LENGTH];
		string	strMSVRName = "";
		
		SmtMSVRJobData		*pJobData = NULL;
		SmtMapServerDevice	*pMapServerDevice = NULL;
	
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MAPSEVER_LOG_NAME);
		if (pLog == NULL)
			return SMT_ERR_FAILURE;

		SmtMapServerDeviceMgr * pMapServerDeviceMgr = SmtMapServerDeviceMgr::GetSingletonPtr();
		SmtMapServiceMgr * pMapServiceMgr = SmtMapServiceMgr::GetSingletonPtr();
		
		for (;;)
		{
			this->m_sServer.ReceiveFrom(revBuf, sizeof(revBuf), from);

			strAddr = TransAddr(from,port);

			if (pLog != NULL)
			{
				pLog->LogMessage("request from(address = %s,port = %d)",strAddr.c_str(),port);
			}

			string strRequest = revBuf;
			vector<string> vStrTmp;
			if (STR_Tokenize(strRequest,vStrTmp,"&") < 1)
				continue;

			pJobData = new SmtMSVRJobData;
			pMapServerDevice = pMapServerDeviceMgr->CreateMapServerDevice("SmtMapServerDevice1.1.1");

			//////////////////////////////////////////////////////////////
			pLog->LogMessage("address = %s,port = %d",strAddr.c_str(),port);

			sprintf_s(szJobName,SMT_JOBNAME_LENGTH,"%d",port);
			pMapServerDevice->SetJobName(szJobName);
			pMapServerDevice->SetJobNo(port);
			
			//////////////////////////////////////////////////////////////
			for (int i = 0; i < vStrTmp.size();i += 2)
			{
				if (pJobData->mapKey2Value[vStrTmp[i]] == "" )
					pJobData->mapKey2Value[vStrTmp[i]] = vStrTmp[i+1];
				else
					pJobData->mapKey2Value[vStrTmp[i]] += ( " " + vStrTmp[i+1] );
			}
			
			strMSVRName =  pJobData->mapKey2Value[c_str_msvr_name];
			pJobData->pSktServer = &m_sServer;
			pJobData->requestAdr = from;
			pJobData->pMapService = pMapServiceMgr->GetMapService(strMSVRName.c_str());

			m_pThreadPool->Run(pMapServerDevice,pJobData);

			pLog->LogMessage("busy = %d,idle = %d,avail = %d",strAddr.c_str(),
				m_pThreadPool->GetBusyNum(),
				m_pThreadPool->GetIdleNum(),
				m_pThreadPool->GetActualAvailNum());

			cout << "busy = "  << m_pThreadPool->GetBusyNum() 
				 << ",idle = " << m_pThreadPool->GetIdleNum()
				 << ",avail = "<< m_pThreadPool->GetActualAvailNum() 
				 << ",all = "  << m_pThreadPool->GetAllNum() << endl;
		}

		return SMT_ERR_NONE;
	}
}