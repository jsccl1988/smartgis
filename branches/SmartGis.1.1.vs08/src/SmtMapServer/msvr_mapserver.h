/*
File:    smt_mapserver.h

Desc:    SmartGis ,并发地图服务器

Version: Version 1.0

Writter:  陈春亮

Date:    2012.7.23

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVER_H
#define _MSVR_MAPSERVER_H

#include "msvr_core.h"
#include "msvr_mapservice.h"
#include "msvr_mapserverdevice.h"
#include "nt_socket.h"

using namespace Smt_MapService;
using namespace Smt_NetWork;

namespace Smt_MapServer
{
	class SMT_EXPORT_CLASS SmtMapServer
	{
	public:
		SmtMapServer();
		~SmtMapServer();

	public:
		long				Init(uint unPort = 9001);
		long				Create(void);
		long				Destroy(void);

	public:
		long				Run(void);

	protected:
		SmtSocket			m_sServer;
		uint				m_unPort;

		SmtThreadPool		*m_pThreadPool;
	};
}

#if !defined(Export_SmtMapServer)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerD.lib")
#       else
#          pragma comment(lib,"SmtMapServer.lib")
#	    endif
#endif

#endif //_MSVR_MAPSERVER_H