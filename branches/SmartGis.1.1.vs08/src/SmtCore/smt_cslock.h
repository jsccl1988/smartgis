/*
File:    smt_cslock.h

Desc:    SmartGis , Critical Sections Lock,加入先自旋锁，如果是单核cpu则无效

Version: Version 1.0

Writter:  陈春亮

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_CSLOCK_H
#define _SMT_CSLOCK_H

#include "smt_core.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtCSLock  
	{
	public:
		SmtCSLock(unsigned long ulSpinCount = 10);
		virtual ~SmtCSLock();

	public:
		bool					TryLock();
		void					Lock();
		void					Unlock();

	public:
		static void				SafeChange(long  *Target, long Value);
		
	private:
		 CRITICAL_SECTION		m_critSection;
	};

	//////////////////////////////////////////////////////////////////////////
	/*
	Scoped MT-exclusive lock
	//example:
		SmtCSLock			cslock;
		SmtScopeCSLock		scope(&cslock);
		锁住lock生命周期
	/////////////////////////////////////////////////////////////////////////*/
	class  SMT_EXPORT_CLASS SmtScopeCSLock 
	{
	public:
		SmtScopeCSLock (SmtCSLock * pCSLock) : m_pCSLock (pCSLock)		{ m_pCSLock->Lock();   }
		~SmtScopeCSLock()												{ m_pCSLock->Unlock(); }
	private:
		SmtCSLock *				m_pCSLock;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_CSLOCK_H