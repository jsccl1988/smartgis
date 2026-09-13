/*
File:    smt_srwlock.h

Desc:    SmartGis ,��д��

Version: Version 1.0

Writter:  �´���

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_SRWLOCK_H
#define _SMT_SRWLOCK_H

#include "base/core/core.h"

namespace base
{
	class CORE_EXPORT SmtSRWLock  
	{
	public:
		SmtSRWLock();
		virtual ~SmtSRWLock();

	public:
		void					lock_exclusive();
		void					unlock_exclusive();

		void					lock_shared();
		void					unlock_shared();
	
	private:
		SRWLOCK					m_srwlock;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-share lock
	//example:
		SmtSRWLock					srwlock;
		SmtScopeSRWLockShared		scope(&srwlock);
		��סlock��������
	*/
	/////////////////////////////////////////////////////////////////////////
	class  CORE_EXPORT SmtScopeSRWLockShared 
	{
	public:
		SmtScopeSRWLockShared (SmtSRWLock * pSRWLock) : m_pSRWLock (pSRWLock)		{ m_pSRWLock->lock_shared();   }
		~SmtScopeSRWLockShared()													{ m_pSRWLock->unlock_shared(); }
	private:
		SmtSRWLock *				m_pSRWLock;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-exclusive lock
	//example:
	SmtSRWLock					srwlock;
	SmtScopeSRWLockShared		scope(&srwlock);
	��סlock��������
	*/
	/////////////////////////////////////////////////////////////////////////
	class  CORE_EXPORT SmtScopeSRWLockExclusive 
	{
	public:
		SmtScopeSRWLockExclusive (SmtSRWLock * pSRWLock) : m_pSRWLock (pSRWLock)		{ m_pSRWLock->lock_exclusive();   }
		~SmtScopeSRWLockExclusive()														{ m_pSRWLock->unlock_exclusive(); }
	private:
		SmtSRWLock *				m_pSRWLock;
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_SRWLOCK_H