#include "base/core/cslock.h"
namespace base
{
	SmtCSLock::SmtCSLock(unsigned long ulSpinCount)
	{
		//InitializeCriticalSection(&m_critSection );
		InitializeCriticalSectionAndSpinCount(&m_critSection,ulSpinCount);
	}

	SmtCSLock::~SmtCSLock()
	{
		DeleteCriticalSection(&m_critSection);
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtCSLock::try_lock()
	{
		return TryEnterCriticalSection(&m_critSection)?true:false;
	}

	void SmtCSLock::lock()
	{
		EnterCriticalSection(&m_critSection);
	}

	void SmtCSLock::unlock()
	{
		LeaveCriticalSection(&m_critSection );
	}

	void SmtCSLock::Lock() { lock(); }

	void SmtCSLock::Unlock() { unlock(); }

	void SmtCSLock::safe_change(long  *Target, long Value)
	{
		//ԭ�ӷ���
		InterlockedExchange (Target, Value);
	}
}