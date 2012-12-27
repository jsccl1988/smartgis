#include "smt_srwlock.h"

namespace Smt_Core
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
	void SmtSRWLock::LockExclusive()
	{
		//AcquireSRWLockExclusive(&m_srwlock);
	}

	void SmtSRWLock::UnlockExclusive()
	{
		//ReleaseSRWLockExclusive(&m_srwlock);
	}
 
	void SmtSRWLock::LockShared()
	{
		//AcquireSRWLockShared(&m_srwlock);
	}

	void SmtSRWLock::UnlockShared()
	{
		//ReleaseSRWLockShared(&m_srwlock);
	}
}