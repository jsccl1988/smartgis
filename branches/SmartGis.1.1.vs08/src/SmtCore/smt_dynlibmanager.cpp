#include "smt_dynlibmanager.h"
#include "smt_logmanager.h"

namespace Smt_Core
{
	extern const string g_strDynLibLog;
	SmtDynLibManager* SmtDynLibManager::m_pSingleton = NULL;

	SmtDynLibManager::SmtDynLibManager(void)
	{
           SmtLogManager::GetSingletonPtr()->CreateLog(g_strDynLibLog.c_str());
	}

	SmtDynLibManager::~SmtDynLibManager(void)
	{
		// Unload & delete resources in turn
		for( vSmtDynLibPtrs::iterator it = m_vDynLibPtrs.begin(); it != m_vDynLibPtrs.end(); ++it )
		{
			(*it)->UnLoad();
			SMT_SAFE_DELETE( *it);
		}

		// Empty the list
		m_vDynLibPtrs.clear();
	}

	SmtDynLibManager* SmtDynLibManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtDynLibManager();
		}
		return m_pSingleton;
	}

	void SmtDynLibManager::DestoryInstance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtDynLib* SmtDynLibManager::LoadDynLib(const char * name,const char * path)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		SmtDynLib* pLib = NULL;
	
		vSmtDynLibPtrs::iterator i = m_vDynLibPtrs.begin();	
		while (i != m_vDynLibPtrs.end())
		{			
			if (strcmp((*i)->GetName(),name) == 0)
			{
				pLib = *i;
				return pLib;
			}
			++i;
		}

		pLib = new SmtDynLib(name,path);
		if(pLib->Load())
			m_vDynLibPtrs.push_back(pLib);
		else
			SMT_SAFE_DELETE(pLib);

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return pLib;
	}

	void SmtDynLibManager::UnLoadDynLib(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vSmtDynLibPtrs::iterator i = m_vDynLibPtrs.begin();	
		while (i != m_vDynLibPtrs.end())
		{			
			if (strcmp((*i)->GetName(),name) == 0)
			{
				(*i)->UnLoad();
				SMT_SAFE_DELETE(*i);
				m_vDynLibPtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}
}