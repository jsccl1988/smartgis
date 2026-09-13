#include "base/core/api.h"
#include "sys/sysmanager.h"
#include "base/core/core_exception.h"

namespace sys
{
	SmtSysManager* SmtSysManager::m_pSingleton = NULL;

	SmtSysManager* SmtSysManager::get_singleton_ptr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtSysManager();
		}
		return m_pSingleton;
	}

	void SmtSysManager::destroy_instance(void)
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