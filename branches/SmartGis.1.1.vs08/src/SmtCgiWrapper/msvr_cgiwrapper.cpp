#include "msvr_cgiwrapper.h"
#include "smt_logmanager.h"
#include "msvr_api.h"

using namespace std;
using namespace Smt_Core;

namespace Smt_CgiWrapper
{
	SmtCgiWrapper::SmtCgiWrapper():m_unSvrPort(9001)
	{

	}

	SmtCgiWrapper::~SmtCgiWrapper()
	{
		m_sClient.Close();
		Destroy();
	}

	long SmtCgiWrapper::Init(uint unSvrPort)
	{
		m_unSvrPort = unSvrPort;

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->CreateLog(MSVR_CGIWRAPPER_LOG_NAME);

		if (NULL == pLog)
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtCgiWrapper::Create()
	{
		m_svrAdr = TransAddr("localhost", m_unSvrPort);

		//创建网络连接
		int nBufLen = 0;	//接收数据缓冲区大小
		int nOptlen = sizeof(nBufLen);
		int uiNewRcvBuf;

		if (SMT_ERR_NONE == this->m_sClient.Create(SOCK_DGRAM) &&
			SMT_ERR_NONE == m_sClient.GetSockOpt( SO_RCVBUF,(char*)&nBufLen, nOptlen,SOL_SOCKET) &&					//获取接收数据缓冲区大小
			(true,nBufLen = MSVR_BUF_LENGTH_64K) &&
			SMT_ERR_NONE == m_sClient.SetSockOpt( SO_RCVBUF,(char*)&nBufLen, nOptlen,SOL_SOCKET) &&					//设置接收数据缓冲区为原来的10倍
			SMT_ERR_NONE == m_sClient.GetSockOpt( SO_RCVBUF,(char*)&uiNewRcvBuf, nOptlen,SOL_SOCKET) &&
			uiNewRcvBuf == nBufLen )													//检查设置系统接收数据缓冲区是否成功
		{
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtCgiWrapper::Destroy()
	{
		return SMT_ERR_NONE;
	}

	long SmtCgiWrapper::Process(const std::map<string,string> &mapKey2Value)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(MSVR_CGIWRAPPER_LOG_NAME);
		if (pLog != NULL)
		{
			map<string,string>::const_iterator iter = mapKey2Value.begin();
			while (iter != mapKey2Value.end())
			{
				pLog->LogMessage("%s=%s",iter->first.c_str(),iter->second.c_str());
				++iter;
			}
		}

		if (SMT_ERR_NONE == SendRequest(mapKey2Value) &&
			SMT_ERR_NONE == RecvResponse())
		{
			return SMT_ERR_NONE;
		}
		
		return SMT_ERR_FAILURE;
	}

	long SmtCgiWrapper::SendRequest(const std::map<string,string> &mapKey2Value)
	{
		char szRequestBuf[MSVR_BUF_LENGTH_1K];
		memset(szRequestBuf,0,MSVR_BUF_LENGTH_1K);

		string strKeyValue;
		map<string,string>::const_iterator iter = mapKey2Value.begin();
		while (iter != mapKey2Value.end())
		{
			strKeyValue += iter->first;
			strKeyValue += "&";
			strKeyValue += iter->second;
			strKeyValue += "&";
			++iter;
		}

		strKeyValue = strKeyValue.substr(0,strKeyValue.size()-1);		//去掉最后的&

		sprintf_s(szRequestBuf,sizeof(szRequestBuf),"%s",strKeyValue.c_str());

		int nSendFlag = m_sClient.SendTo(szRequestBuf, sizeof(szRequestBuf),m_svrAdr);

		return SMT_ERR_NONE;
	}

	long SmtCgiWrapper::RecvResponse()
	{
		char szResponseBuf[MSVR_BUF_LENGTH_32K];
		memset(szResponseBuf,0,MSVR_BUF_LENGTH_32K);

		long	lDataSize = 0;
		long	lType = MSVR_RSP_END;
		char	*pDataBuf = NULL;
		int		nRecvFlag = 0;

		while(szResponseBuf[0] != MSVR_RSP_END && -1 != nRecvFlag)
		{
			nRecvFlag = this->m_sClient.ReceiveFrom(szResponseBuf, sizeof(szResponseBuf),m_svrAdr);

			switch (szResponseBuf[0])
			{
			case MSVR_RSP_TEXT:
				{
					long	lBlockSize = *(long*)(szResponseBuf+sizeof(char));
					pDataBuf = (char *)realloc(pDataBuf, sizeof(char) * (lDataSize+lBlockSize));
					memcpy(pDataBuf+lDataSize,szResponseBuf+ sizeof(char) + sizeof(long),lBlockSize);
					lDataSize += lBlockSize;
					lType  = MSVR_RSP_TEXT;
					break;
				}
			case MSVR_RSP_BIN:
				{
					long	lBlockSize = *(long*)(szResponseBuf+sizeof(char));
					pDataBuf = (char *)realloc(pDataBuf, sizeof(char) * (lDataSize+lBlockSize));
					memcpy(pDataBuf+lDataSize,szResponseBuf+ sizeof(char) + sizeof(long),lBlockSize);
					lDataSize += lBlockSize;
					lType = MSVR_RSP_BIN;
					break;
				}
			case MSVR_RSP_MIME:
				{
					long	lBlockSize = *(long*)(szResponseBuf+sizeof(char));
					pDataBuf = (char *)realloc(pDataBuf, sizeof(char) * (lDataSize+lBlockSize));
					memcpy(pDataBuf+lDataSize,szResponseBuf+ sizeof(char) + sizeof(long),lBlockSize);
					lDataSize += lBlockSize;
					lType = MSVR_RSP_MIME;
					break;
				}
			}
		}

		if (lType == MSVR_RSP_BIN)
			msvrPrintBin(pDataBuf,lDataSize);
		else if(lType == MSVR_RSP_MIME)
			msvrPrintMime(pDataBuf,lDataSize);
		else if(lType == MSVR_RSP_TEXT)
			msvrPrintText(pDataBuf);
		
		if (pDataBuf)
		{
			free(pDataBuf);
			pDataBuf = NULL;
		}

		return SMT_ERR_NONE;
	}
}