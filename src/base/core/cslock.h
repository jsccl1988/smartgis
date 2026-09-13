/*
File:    smt_cslock.h

Desc:    SmartGis , Critical Sections Lock,������������������ǵ���cpu����Ч

Version: Version 1.0

Writter:  �´���

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_CSLOCK_H
#define _SMT_CSLOCK_H

#include "base/core/core.h"

namespace base
{
	class CORE_EXPORT SmtCSLock  
	{
	public:
		SmtCSLock(unsigned long ulSpinCount = 10);
		virtual ~SmtCSLock();

	public:
		bool					try_lock();
		void					lock();
		void					unlock();
		void					Lock();
		void					Unlock();

	public:
		static void				safe_change(long  *Target, long Value);
		
	private:
		 CRITICAL_SECTION		m_critSection;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-exclusive lock
	//example:
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);
		��סlock��������
	/////////////////////////////////////////////////////////////////////////*/
	class  CORE_EXPORT SmtScopeCSLock 
	{
	public:
		SmtScopeCSLock (SmtCSLock * pCSLock) : m_pCSLock (pCSLock)		{ m_pCSLock->lock();   }
		~SmtScopeCSLock()												{ m_pCSLock->unlock(); }
	private:
		SmtCSLock *				m_pCSLock;
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_CSLOCK_H