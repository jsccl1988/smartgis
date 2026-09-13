#include "stdafx.h"
#include "ui/xcatalog/scenemgr.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "base/core/api.h"
#include "ui/xcatalog/mapdocxcatalog.h"
#include "ui/xcatalog/3dobjxcatalog.h"
#include "tool/t_msg.h"
#include <algorithm>

using namespace sdb;
using namespace sdb;

namespace ui
{
	SmtSceneMgr* SmtSceneMgr::m_pSingleton = NULL;

	SmtSceneMgr* SmtSceneMgr::get_singleton_ptr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtSceneMgr();
		}

		return m_pSingleton;
	}

	void SmtSceneMgr::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtSceneMgr::SmtSceneMgr(void)
	{
		m_pScene = NULL;
	}

	SmtSceneMgr::~SmtSceneMgr(void)
	{
		m_v3DObjCatalogPtrs.clear();
		m_v3DXViewPtrs.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	SmtScene *SmtSceneMgr::GetScenePtr(void)
	{
		return m_pScene;
	}

	const SmtScene * SmtSceneMgr::GetScenePtr(void) const
	{
		return m_pScene;
	}

	SmtScene& SmtSceneMgr::GetScene(void)
	{
		return *m_pScene;
	}

	const SmtScene& SmtSceneMgr::GetScene(void) const
	{
		return *m_pScene;
	}

	bool SmtSceneMgr::AttachScene(SmtScene *pScene)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		m_pScene = pScene;

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return Update3DObjCatalog();
	}

	SmtScene * SmtSceneMgr::DettachScene(void)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		SmtScene * pScene = m_pScene;
		m_pScene = NULL;

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		Update3DObjCatalog();

		return pScene;
	}
	//////////////////////////////////////////////////////////////////////////
	bool SmtSceneMgr::Register3DXView(void*p3DXView)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v3DXViewPtrs.begin(),m_v3DXViewPtrs.end(),p3DXView); 

		if(iter ==m_v3DXViewPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v3DXViewPtrs.push_back(p3DXView); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtSceneMgr::Unregister3DXView(void*p3DXView)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v3DXViewPtrs.begin(),m_v3DXViewPtrs.end(),p3DXView); 

		if(iter!=m_v3DXViewPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v3DXViewPtrs.erase(iter); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtSceneMgr::Update3DXView(void)
	{
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtSceneMgr::Register3DObjCatalog(void*p3DObjCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v3DObjCatalogPtrs.begin(),m_v3DObjCatalogPtrs.end(),p3DObjCatalog); 

		if(iter ==m_v3DObjCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v3DObjCatalogPtrs.push_back(p3DObjCatalog); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtSceneMgr::Unregister3DObjCatalog(void*p3DObjCatalog)
	{
		vector<void*>::iterator iter; 
		iter = find(m_v3DObjCatalogPtrs.begin(),m_v3DObjCatalogPtrs.end(),p3DObjCatalog); 

		if(iter!=m_v3DObjCatalogPtrs.end()) 
		{

#ifdef SMT_THREAD_SAFE
			m_cslock.Lock();
#endif

			m_v3DObjCatalogPtrs.erase(iter); 

#ifdef SMT_THREAD_SAFE
			m_cslock.Unlock();
#endif
			return true;
		}

		return false;
	}

	bool SmtSceneMgr::Update3DObjCatalog(void)
	{
		Smt3DObjXCatalog *p3DObjCatalog;
		vector<void*>::iterator iter = m_v3DObjCatalogPtrs.begin();
		while(iter!=m_v3DObjCatalogPtrs.end())
		{
			p3DObjCatalog = (Smt3DObjXCatalog *)(*iter);
			p3DObjCatalog->Update3DObjTree();
			++iter;
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtSceneMgr::CreateOctTreeSceneMgr(void)
	{
		if (m_pScene)
			return m_pScene->CreateOctTreeSceneMgr();
	}

	void SmtSceneMgr::Add3DObject(Smt3DObject* p3DObject)
	{
		if (m_pScene)
		{
			m_pScene->Add3DObject(p3DObject);
			Update3DObjCatalog();
		}
	}

	void SmtSceneMgr::Remove3DObject(int index)
	{
		if (m_pScene)
		{
			m_pScene->Remove3DObject(index);
			Update3DObjCatalog();
		}
	}

	void SmtSceneMgr::Remove3DObject(Smt3DObject* p3DObject)
	{
		if (m_pScene)
		{
			m_pScene->Remove3DObject(p3DObject);
			Update3DObjCatalog();
		}
	}

	Smt3DObject* SmtSceneMgr::Get3DObject(int index)
	{
		if (m_pScene)
			return m_pScene->Get3DObject(index);

		return NULL;
	}

	void SmtSceneMgr::Get3DObjectPtrs(vSmt3DObjectPtrs &v3DObjectPtrs)
	{
		if (m_pScene)
			m_pScene->Get3DObjectPtrs(v3DObjectPtrs);
	}
}