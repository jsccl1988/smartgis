/*
File:    cata_mapservicemgr.h

Desc:    SmtMapServiceMgr,地图服务管理类

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.3

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_MAPSERVICEMGR_H
#define _CATA_MAPSERVICEMGR_H

#include "smt_core.h"
#include "msvr_mapservice.h"
#include "smt_env_struct.h"

#include "smt_cslock.h"

using namespace Smt_MapService;

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtMapServiceMgr
	{
	private:
		SmtMapServiceMgr(void);

	public:
		virtual ~SmtMapServiceMgr(void);

	public:
		static SmtMapServiceMgr*		GetSingletonPtr(void);
		static void						DestoryInstance(void);

	public:
		bool							OpenMSVRCfg(const char *szCfgFile);
		bool							SaveMSVRCfg(const char *szCfgFile = "");
		bool							CreateMapService();

	public:
		bool							AppendMapService(SmtMapService *pMapService);

		bool							RemoveMapService(const char *szMapServiceName);
		bool							RemoveMapService(SmtMapService *pMapService);

		SmtMapService*					GetMapService(int index);
		const SmtMapService*			GetMapService(int index) const;

		SmtMapService*					GetMapService(const char *szMapServiceName);
		const SmtMapService*			GetMapService(const char *szMapServiceName) const;

		int								GetMapServiceCount(void) {return m_vMapServices.size();}

		void							MoveFirst(void) const ;
		void							MoveNext(void) const ;
		void							MoveLast(void) const ;
		void							Delete(void);
		bool							IsEnd(void) const ;

		void							DeleteAll(void);

		SmtMapService *					GetMapService();
		const SmtMapService *			GetMapService() const ;

	public:
		bool							RegisterMServiceCatalog(void *pMServiceCatalog);
		bool							UnregisterMServiceCatalog(void*pMServiceCatalog);


		bool							UpdateMServiceCatalog(void);

	protected:
		string							m_strMSVRCfg;
	
		vector<void*>					m_vMServiceCatalogPtrs;

#ifdef SMT_THREAD_SAFE
		SmtCSLock						m_cslock;										//多线程安全
#endif

		vector<SmtMapService*>			m_vMapServices;
		SmtMapService*					m_pCurrentMapService;
		mutable int						m_nIteratorIndex;
	private:
		static SmtMapServiceMgr*		m_pSingleton;
	};
}

#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_MAPSERVICEMGR_H