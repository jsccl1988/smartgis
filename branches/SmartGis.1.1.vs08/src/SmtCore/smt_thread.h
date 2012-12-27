/*
File:    smt_thread.h

Desc:    SmartGis ,多线程基类,不支持MFC

Version: Version 1.0

Writter:  陈春亮

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_THREAD_H
#define _SMT_THREAD_H
#include "smt_core.h"
#include "smt_cslock.h"
#include "smt_bas_struct.h"

#include <process.h>

namespace Smt_Core
{
	enum SmtThreadState
	{//线程状态
		SMT_THREAD_IDLE =0,
		SMT_THREAD_RUNNING,
		SMT_THREAD_SUSPENDED,
		SMT_THREAD_TERMINATED,
		SMT_THREAD_FINISHED,
		SMT_THREAD_DEAD
	};

	enum SmtThreadPriority
	{//线程优先级
		SMT_THREAD_PRIORITY_LOWEST = 0,        
		SMT_THREAD_PRIORITY_BELOW_NORMAL,        
		SMT_THREAD_PRIORITY_NORMAL,        
		SMT_THREAD_PRIORITY_ABOVE_NORMAL,        
		SMT_THREAD_PRIORITY_HIGHEST,        
	};

	class SMT_EXPORT_CLASS SmtThread
	{
	public:
		SmtThread();
		virtual ~SmtThread();

	public:
		inline operator					HANDLE() const{ return m_hThread;}
		inline HANDLE					GetThread() const{return m_hThread;}
		inline uint						GetThreadID() const{return m_uiThreadID;}

	public:
		void							SetState(SmtThreadState state){m_trdState = state;}
		SmtThreadState					SetState(void){return m_trdState;}

		void							SetPriority(SmtThreadPriority priority);
		SmtThreadPriority				GetPriority();
		
		bool							Start(void	*pParam = NULL);
		void							Suspend();
		void							Resume();
		ulong							Wait(DWORD dwMilliseconds = INFINITE);
		bool							Kill();

		virtual void					Run(void *pParam) = 0;

		static void						Sleep(uint delay = 0);

	protected:
		static uint  __stdcall			Proxy(void *pThread);

	protected:
		HANDLE							m_hThread;
		uint							m_uiThreadID;
		bool							m_bStarted;
		bool							m_bDetached;

		SmtCSLock						m_cslock;
		SmtThreadState					m_trdState;		
		void							*m_pParam;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_THREAD_H