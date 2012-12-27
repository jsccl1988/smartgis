/*
File:    msvr_mapserverdevice111.h

Desc:    SmartGis ,并发地图服务器驱动

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVERDEVICE111_H
#define _MSVR_MAPSERVERDEVICE111_H

#include "smt_threadpool.h"
#include "msvr_mapservice.h"
#include "msvr_core.h"
#include "smt_cslock.h"
#include "nt_socket.h"
#include "msvr_mapserverdevice.h"

#include <map>

using namespace Smt_MapService;
using namespace Smt_NetWork;

namespace Smt_MapServer
{
	class SMT_EXPORT_CLASS SmtMapServerDevice111:public SmtMapServerDevice
	{
	public:
		SmtMapServerDevice111();
		virtual ~SmtMapServerDevice111();

	public:
		virtual long	GetVersion(void) {return MSVR_VERSION_1_1_1;};

	protected:
		//wms
		virtual long	WMSGetMap(void);
		virtual long	WMSGetCapabilities(void);
		virtual long	WMSGetFeatureInfo(void);

		//wmts
		virtual long	WMTSGetTile(void);
		virtual long	WMTSGetCapabilities(void);
		virtual long	WMTSGetFeatureInfo(void);

		//wfs
		virtual long	WFSGetLayer(void);
		virtual long	WFSGetFeature(void);
		virtual long	WFSGetCapabilities(void);
	};
}

#if !defined(Export_SmtMapServerDevice111)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerDevice111D.lib")
#       else
#          pragma comment(lib,"SmtMapServerDevice111.lib")
#	    endif
#endif

#endif //_MSVR_MAPSERVERDEVICE111_H
