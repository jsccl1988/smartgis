/*
File:    smt_thread.h

Desc:    SmartGis ,���̻߳���,��֧��MFC

Version: Version 1.0

Writter:  �´���

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_THREAD_H
#define _SMT_THREAD_H
#include "base/core/core.h"
#include "base/core/cslock.h"
#include "base/core/bas_struct.h"

#include <process.h>

namespace base
{
	enum SmtThreadState
	{//�߳�״̬
		SMT_THREAD_IDLE =0,
		SMT_THREAD_RUNNING,
		SMT_THREAD_SUSPENDED,
		SMT_THREAD_TERMINATED,
		SMT_THREAD_FINISHED,
		SMT_THREAD_DEAD
	};

	enum SmtThreadPriority
	{//�߳����ȼ�
		SMT_THREAD_PRIORITY_LOWEST = 0,        
		SMT_THREAD_PRIORITY_BELOW_NORMAL,        
		SMT_THREAD_PRIORITY_NORMAL,        
		SMT_THREAD_PRIORITY_ABOVE_NORMAL,        
		SMT_THREAD_PRIORITY_HIGHEST,        
	};

	class CORE_EXPORT SmtThread
	{
	public:
		SmtThread();
		virtual ~SmtThread();

	public:
		inline operator					HANDLE() const{ return m_hThread;}
		inline HANDLE					get_thread() const{return m_hThread;}
		inline uint						get_thread_id() const{return m_uiThreadID;}

	public:
		void							set_state(SmtThreadState state){m_trdState = state;}
		SmtThreadState					set_state(void){return m_trdState;}

		void							set_priority(SmtThreadPriority priority);
		SmtThreadPriority				get_priority();
		
		bool							start(void	*pParam = NULL);
		void							suspend();
		void							resume();
		ulong							wait(DWORD dwMilliseconds = INFINITE);
		bool							kill();

		virtual void					run(void *pParam) = 0;

		static void						sleep(uint delay = 0);

	protected:
		static uint  __stdcall			proxy(void *pThread);

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

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_THREAD_H