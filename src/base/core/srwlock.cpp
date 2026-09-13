#include "base/core/srwlock.h"

namespace base
{
	SmtSRWLock::SmtSRWLock()
	{
		//InitializeSRWLock(&m_srwlock);
	}

	SmtSRWLock::~SmtSRWLock()
	{
		;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtSRWLock::lock_exclusive()
	{
		//AcquireSRWLockExclusive(&m_srwlock);
	}

	void SmtSRWLock::unlock_exclusive()
	{
		//ReleaseSRWLockExclusive(&m_srwlock);
	}
 
	void SmtSRWLock::lock_shared()
	{
		//AcquireSRWLockShared(&m_srwlock);
	}

	void SmtSRWLock::unlock_shared()
	{
		//ReleaseSRWLockShared(&m_srwlock);
	}
}