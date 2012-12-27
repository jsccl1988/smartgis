/*
File:    msvr_mapserverdevice.h

Desc:    SmartGis ,并发地图服务器驱动

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVERDEVICE_H
#define _MSVR_MAPSERVERDEVICE_H

#include "smt_threadpool.h"
#include "msvr_mapservice.h"
#include "msvr_core.h"
#include "smt_cslock.h"
#include "nt_socket.h"

#include <map>

using namespace Smt_MapService;
using namespace Smt_NetWork;

namespace Smt_MapServer
{
	struct SmtMSVRJobData 
	{
		SmtSocket							*pSktServer;
		SmtMapService						*pMapService;
		std::map<string,string>				mapKey2Value;
		SOCKADDR_IN							requestAdr;

		SmtMSVRJobData():pMapService(NULL)
			,pSktServer(NULL)
		{
			;
		}
	};

	class SMT_EXPORT_CLASS SmtMapServerDevice:public Smt_Core::SmtJob
	{
	public:
		SmtMapServerDevice();
		virtual ~SmtMapServerDevice();

	public:
		virtual void	Run ( void *ptr );
		virtual long	GetVersion(void) = 0;

	protected:
		long			Process(void);

	protected:
		//wms
		long			WMSDispatch(void);

		virtual long	WMSGetMap(void) = 0;
		virtual long	WMSGetCapabilities(void) = 0;
		virtual long	WMSGetFeatureInfo(void) = 0;

		//wmts
		long			WMTSDispatch(void);

		virtual long	WMTSGetTile(void) = 0;
		virtual long	WMTSGetCapabilities(void) = 0;
		virtual long	WMTSGetFeatureInfo(void) = 0;

		//wfs
		long			WFSDispatch(void);

		virtual long	WFSGetLayer(void) = 0;
		virtual long	WFSGetFeature(void) = 0;
		virtual long	WFSGetCapabilities(void) = 0;

		//default
		virtual long	Default(void);

	protected:
		long			SendBuf(const char * pBuffer, long lBufferSize,long type);

	protected:
		SmtMSVRJobData	*m_pJobData;
		SmtCSLock		m_cslock;
	};

	typedef class SmtMapServerDevice *LPMAPSERVERDEVICE;
}

#if !defined(Export_SmtMapServer)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerD.lib")
#       else
#          pragma comment(lib,"SmtMapServer.lib")
#	    endif
#endif


#endif //_MSVR_MAPSERVERDEVICE_H
