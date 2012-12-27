/*
File:    cata_mapmgr.h

Desc:    SmtMapMgr,地图文档管理类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_MAPMGR_H
#define _CATA_MAPMGR_H

#include "smt_core.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "smt_env_struct.h"

#include "smt_cslock.h"

using namespace Smt_GIS;

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtMapMgr
	{
	private:
		SmtMapMgr(void);

	public:
		virtual ~SmtMapMgr(void);

	public:
		static SmtMapMgr*       GetSingletonPtr(void);
		static void             DestoryInstance(void);

	public:
		static bool             NewMap(SmtMap *&pMap,const char *szMapName);
		static bool             OpenMap(SmtMap *pMap,const char *szMapFile);
		static bool             SaveMapAs(SmtMap *pMap,const char *szFilePath);

	public:
		SmtMap *				GetSmtMapPtr(void);
		const SmtMap *			GetSmtMapPtr(void) const;

		SmtMap&					GetSmtMap(void);
		const SmtMap&			GetSmtMap(void) const;

		bool                    NewMap(const char *szMapName);
		bool                    OpenMap(const char *szMapFile);
		bool                    CloseMap();
		bool                    SaveMap();
		bool                    SaveMapAs(const char *szFilePath);

		bool                    AppendLayer(SmtLayer *pLayer);
		bool                    DeleteLayer(const char *szName);
		SmtLayer*				GetLayer(int index);
		SmtLayer*				GetLayer(const char *szName);

		bool                    SetActiveLayer(const char *szName);
		SmtLayer*				GetActiveLayer(void);

		bool					AppendFeature(SmtFeature *pFeature,bool bIsClone = false);

	public:
		bool					Register2DXView(void *p2DXView);
		bool					Unregister2DXView(void*p2DXView);

		bool					RegisterMapCatalog(void *pMapCatalog);
		bool					UnregisterMapCatalog(void*pMapCatalog);

	protected:
		bool					Update2DXView(void);
		bool					UpdateMapCatalog(void);

	private:
		SmtMap					*m_pSmtMap;
		string					m_strMDocPath;
		vector<void*>			m_v2DXViewPtrs;
		vector<void*>			m_vMapCatalogPtrs;
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//多线程安全
#endif

	private:
		static SmtMapMgr*       m_pSingleton;
	};
}

#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_MAPMGR_H