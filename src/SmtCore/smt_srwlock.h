/*
File:    smt_srwlock.h

Desc:    SmartGis ,读写锁

Version: Version 1.0

Writter:  陈春亮

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_SRWLOCK_H
#define _SMT_SRWLOCK_H

#include "smt_core.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtSRWLock  
	{
	public:
		SmtSRWLock();
		virtual ~SmtSRWLock();

	public:
		void					LockExclusive();
		void					UnlockExclusive();

		void					LockShared();
		void					UnlockShared();
	
	private:
		SRWLOCK					m_srwlock;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-share lock
	//example:
		SmtSRWLock					srwlock;
		SmtScopeSRWLockShared		scope(&srwlock);
		锁住lock生命周期
	*/
	/////////////////////////////////////////////////////////////////////////
	class  SMT_EXPORT_CLASS SmtScopeSRWLockShared 
	{
	public:
		SmtScopeSRWLockShared (SmtSRWLock * pSRWLock) : m_pSRWLock (pSRWLock)		{ m_pSRWLock->LockShared();   }
		~SmtScopeSRWLockShared()													{ m_pSRWLock->UnlockShared(); }
	private:
		SmtSRWLock *				m_pSRWLock;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-exclusive lock
	//example:
	SmtSRWLock					srwlock;
	SmtScopeSRWLockShared		scope(&srwlock);
	锁住lock生命周期
	*/
	/////////////////////////////////////////////////////////////////////////
	class  SMT_EXPORT_CLASS SmtScopeSRWLockExclusive 
	{
	public:
		SmtScopeSRWLockExclusive (SmtSRWLock * pSRWLock) : m_pSRWLock (pSRWLock)		{ m_pSRWLock->LockExclusive();   }
		~SmtScopeSRWLockExclusive()														{ m_pSRWLock->UnlockExclusive(); }
	private:
		SmtSRWLock *				m_pSRWLock;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_SRWLOCK_H