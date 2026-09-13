#include "base/core/dynlibmanager.h"
#include "base/core/logmanager.h"

namespace base
{
	extern const string g_strDynLibLog;
	SmtDynLibManager* SmtDynLibManager::m_pSingleton = NULL;

	SmtDynLibManager::SmtDynLibManager(void)
	{
           SmtLogManager::get_singleton_ptr()->create_log(g_strDynLibLog.c_str());
	}

	SmtDynLibManager::~SmtDynLibManager(void)
	{
		// Unload & delete resources in turn
		for( vSmtDynLibPtrs::iterator it = m_vDynLibPtrs.begin(); it != m_vDynLibPtrs.end(); ++it )
		{
			(*it)->unload();
			SMT_SAFE_DELETE( *it);
		}

		// Empty the list
		m_vDynLibPtrs.clear();
	}

	SmtDynLibManager* SmtDynLibManager::get_singleton_ptr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtDynLibManager();
		}
		return m_pSingleton;
	}

	void SmtDynLibManager::destroy_instance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtDynLib* SmtDynLibManager::load_dyn_lib(const char * name,const char * path)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		SmtDynLib* pLib = NULL;
	
		vSmtDynLibPtrs::iterator i = m_vDynLibPtrs.begin();	
		while (i != m_vDynLibPtrs.end())
		{			
			if (strcmp((*i)->get_name(),name) == 0)
			{
				pLib = *i;
				return pLib;
			}
			++i;
		}

		pLib = new SmtDynLib(name,path);
		if(pLib->load())
			m_vDynLibPtrs.push_back(pLib);
		else
			SMT_SAFE_DELETE(pLib);

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		return pLib;
	}

	void SmtDynLibManager::unload_dyn_lib(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		vSmtDynLibPtrs::iterator i = m_vDynLibPtrs.begin();	
		while (i != m_vDynLibPtrs.end())
		{			
			if (strcmp((*i)->get_name(),name) == 0)
			{
				(*i)->unload();
				SMT_SAFE_DELETE(*i);
				m_vDynLibPtrs.erase(i);	
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}
}