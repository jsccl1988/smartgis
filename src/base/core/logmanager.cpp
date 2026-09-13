#include "base/core/api.h"
#include "base/core/logmanager.h"
#include "base/core/core_exception.h"

namespace base
{
	SmtLogManager* SmtLogManager::m_pSingleton = NULL;

	SmtLogManager* SmtLogManager::get_singleton_ptr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtLogManager();
		}
		return m_pSingleton;
	}

	void SmtLogManager::destroy_instance(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////

	SmtLogManager::SmtLogManager(void)
	{
	
	}

	SmtLogManager::~SmtLogManager(void)
	{
		destroy_all_log();
	}

	//////////////////////////////////////////////////////////////////////////

	bool SmtLogManager::set_default_log(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		m_pDefaultLog = new SmtLog(name,m_strLogDir.c_str());

		if (NULL == m_pDefaultLog)
			return false;

		m_vLogPtrs.push_back(m_pDefaultLog);
#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
		return true;
	}

	void SmtLogManager::destroy_all_log()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		vLogPtrs::iterator i = m_vLogPtrs.begin() ;

		while (i != m_vLogPtrs.end())
		{
			SMT_SAFE_DELETE(*i);
			++i;
		}
		m_vLogPtrs.clear();

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}

	SmtLog * SmtLogManager::create_log(const char * name,ios::ios_base::openmode mode)
	{ 
		SmtLog *pLog = NULL;
		pLog = get_log(name);
		if (!pLog)
		{
			try
			{
				pLog = new SmtLog(name,m_strLogDir.c_str(),mode);
			}
			catch (Exception e)
			{
				if (m_pDefaultLog)
					m_pDefaultLog->log_message("create log %s failed",name);
				SMT_SAFE_DELETE(pLog);
				return pLog;
			}

			m_vLogPtrs.push_back(pLog);
		}

		return pLog;
	}

	void SmtLogManager::destroy_log(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		SmtLog * pLog = NULL;
		vLogPtrs::iterator i = m_vLogPtrs.begin() ;

		while (i != m_vLogPtrs.end())
		{
			if ( strcmp((**i).m_strLogName.c_str(),name) == 0)
			{
				SMT_SAFE_DELETE(*i);
				m_vLogPtrs.erase(i);			
				break;
			}
			++i;
		}

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}

	void SmtLogManager::destroy_log(SmtLog *pLog)
	{
		destroy_log(pLog->m_strLogName.c_str());
	}

	SmtLog* SmtLogManager::get_log(const string& name)
	{
		SmtLog * pLog = NULL;
		vLogPtrs::iterator i = m_vLogPtrs.begin() ;

		while (i != m_vLogPtrs.end())
		{
			if (strcmp((**i).m_strLogName.c_str(),name.c_str()) == 0)
			{
				pLog = *i;
				break;
			}
			++i;
		}
		return pLog;
	}

	const SmtLog* SmtLogManager::get_log(const string& name) const
	{
		const SmtLog * pLog = NULL;

		for (int i = 0;i < m_vLogPtrs.size();i++)
		{
			if ((*m_vLogPtrs[i]).m_strLogName == name)
			{
				pLog = m_vLogPtrs[i];
				break;
			}
		}
		
		return pLog;
	}

	SmtLog* SmtLogManager::get_default_log(void)
	{
		if (m_pDefaultLog) 
			return m_pDefaultLog;
		else 
			return NULL;
	}
}