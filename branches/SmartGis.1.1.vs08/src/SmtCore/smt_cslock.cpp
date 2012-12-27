#include "smt_cslock.h"
namespace Smt_Core
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
	bool SmtCSLock::TryLock()
	{
		return TryEnterCriticalSection(&m_critSection)?true:false;
	}

	void SmtCSLock::Lock()
	{
		EnterCriticalSection(&m_critSection);
	}

	void SmtCSLock::Unlock()
	{
		LeaveCriticalSection(&m_critSection );
	}

	void SmtCSLock::SafeChange(long  *Target, long Value)
	{
		//Ô­×Ó·ÃÎÊ
		InterlockedExchange (Target, Value);
	}
}