#include "smt_api.h"
#include "sys_sysmanager.h"
#include "smt_exception.h"

namespace Smt_Sys
{
	SmtSysManager* SmtSysManager::m_pSingleton = NULL;

	SmtSysManager* SmtSysManager::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtSysManager();
		}
		return m_pSingleton;
	}

	void SmtSysManager::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}
	//////////////////////////////////////////////////////////////////////////
	SmtSysManager::SmtSysManager(void)
	{
		
	}

	SmtSysManager::~SmtSysManager(void)
	{
		;
	}
}