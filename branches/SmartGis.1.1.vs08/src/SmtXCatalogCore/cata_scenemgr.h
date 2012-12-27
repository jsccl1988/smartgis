/*
File:    cata_scenemgr.h

Desc:    SmtMapMgr,场景管理类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _CATA_SCENEMGR_H
#define _CATA_SCENEMGR_H

#include "smt_core.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "smt_env_struct.h"
#include "bl3d_scene.h"
#include "smt_cslock.h"

using namespace Smt_GIS;
using namespace Smt_3DBase;

namespace Smt_XCatalog
{
	class SMT_EXPORT_CLASS SmtSceneMgr
	{
	private:
		SmtSceneMgr(void);

	public:
		virtual ~SmtSceneMgr(void);

	public:
		static SmtSceneMgr*     GetSingletonPtr(void);
		static void             DestoryInstance(void);

	public:
		SmtScene *				GetScenePtr(void);
		const SmtScene *		GetScenePtr(void) const;

		SmtScene&				GetScene(void);
		const SmtScene&			GetScene(void) const;

		bool					AttachScene(SmtScene *pScene);
		SmtScene *				DettachScene(void);

	public:
		void					Add3DObject(Smt3DObject* p3DObject);
		Smt3DObject*			Get3DObject(int index);
		void					Remove3DObject(Smt3DObject* p3DObject);
		void					Remove3DObject(int index);
		void					Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs);

		void					CreateOctTreeSceneMgr(void);

	public:
		bool					Register3DXView(void *p3DXView);
		bool					Unregister3DXView(void*p3DXView);

		bool					Register3DObjCatalog(void *p3DObjCatalog);
		bool					Unregister3DObjCatalog(void*p3DObjCatalog);

	protected:
		bool					Update3DXView(void);
		bool					Update3DObjCatalog(void);

	private:
		SmtScene				*m_pScene;
	
		vector<void*>			m_v3DXViewPtrs;
		vector<void*>			m_v3DObjCatalogPtrs;
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//多线程安全
#endif

	private:
		static SmtSceneMgr*       m_pSingleton;
	};
}

#if !defined(Export_SmtXCatalogCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXCatalogCoreD.lib")
#       else
#          pragma comment(lib,"SmtXCatalogCore.lib")
#	    endif
#endif

#endif //_CATA_SCENEMGR_H