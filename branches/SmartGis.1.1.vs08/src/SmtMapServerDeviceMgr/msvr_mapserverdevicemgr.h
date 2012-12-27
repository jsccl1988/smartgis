/*
File:    msvr_mapserverdevicemgr.h

Desc:    SmtDataSourceMgr,地图服务器驱动管理类

Version: Version 1.0

Writer:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_MAPSERVERDEVICEMGR_H
#define _MSVR_MAPSERVERDEVICEMGR_H

#include "msvr_mapserverdevice.h"

namespace Smt_MapServer
{
	class SMT_EXPORT_CLASS SmtMapServerDeviceMgr
	{
	private:
		SmtMapServerDeviceMgr(void);

	public:
		virtual ~SmtMapServerDeviceMgr(void);

	public:
		static SmtMapServerDevice *		CreateMapServerDevice(const char *szMapServerDevice);
		static void						DestoryMapServerDevice(SmtMapServerDevice *&pLayer);
		
	public:
		static SmtMapServerDeviceMgr*	GetSingletonPtr(void);
		static void						DestoryInstance(void);

	private:
		static SmtMapServerDeviceMgr*	m_pSingleton;
	};
}

#if !defined(Export_SmtMapServerDeviceMgr)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMapServerDeviceMgrD.lib")
#       else
#          pragma comment(lib,"SmtMapServerDeviceMgr.lib")
#	    endif  
#endif

#endif //_MSVR_MAPSERVERDEVICEMGR_H