#include "smt_api.h"
#include "smt_logmanager.h"
#include "smt_exception.h"

namespace Smt_Core
{
	SmtLogManager* SmtLogManager::m_pSingleton = NULL;

	SmtLogManager* SmtLogManager::GetSingletonPtr(void)
	{
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);

		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtLogManager();
		}
		return m_pSingleton;
	}

	void SmtLogManager::DestoryInstance(void)
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
		DestroyAllLog();
	}

	//////////////////////////////////////////////////////////////////////////

	bool SmtLogManager::SetDefaultLog(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		m_pDefaultLog = new SmtLog(name,m_strLogDir.c_str());

		if (NULL == m_pDefaultLog)
			return false;

		m_vLogPtrs.push_back(m_pDefaultLog);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return true;
	}

	void SmtLogManager::DestroyAllLog()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		vLogPtrs::iterator i = m_vLogPtrs.begin() ;

		while (i != m_vLogPtrs.end())
		{
			SMT_SAFE_DELETE(*i);
			++i;
		}
		m_vLogPtrs.clear();

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}

	SmtLog * SmtLogManager::CreateLog(const char * name,ios::ios_base::openmode mode)
	{ 
		SmtLog *pLog = NULL;
		pLog = GetLog(name);
		if (!pLog)
		{
			try
			{
				pLog = new SmtLog(name,m_strLogDir.c_str(),mode);
			}
			catch (Exception e)
			{
				if (m_pDefaultLog)
					m_pDefaultLog->LogMessage("create log %s failed",name);
				SMT_SAFE_DELETE(pLog);
				return pLog;
			}

			m_vLogPtrs.push_back(pLog);
		}

		return pLog;
	}

	void SmtLogManager::DestroyLog(const char * name)
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
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
		m_cslock.Unlock();
#endif
	}

	void SmtLogManager::DestroyLog(SmtLog *pLog)
	{
		DestroyLog(pLog->m_strLogName.c_str());
	}

	SmtLog* SmtLogManager::GetLog(const string& name)
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

	const SmtLog* SmtLogManager::GetLog(const string& name) const
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

	SmtLog* SmtLogManager::GetDefaultLog(void)
	{
		if (m_pDefaultLog) 
			return m_pDefaultLog;
		else 
			return NULL;
	}
}