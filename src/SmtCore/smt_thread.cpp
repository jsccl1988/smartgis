#include "smt_thread.h"

namespace Smt_Core
{
	SmtThread::SmtThread():m_hThread(NULL)
		,m_bStarted(false)
		,m_bDetached(false)
		,m_pParam(NULL)
	{
		SetState(SMT_THREAD_IDLE);
	}

	SmtThread::~SmtThread()
	{
		SetState(SMT_THREAD_FINISHED);

		Kill();
	}

	bool SmtThread::Start(void	*pParam)
	{
		if (!m_bStarted)
		{
			m_hThread =(HANDLE)_beginthreadex( NULL,        //security
				0,											//stack size
				Proxy,										//thread func
				(void*)this,								//arg list
				CREATE_SUSPENDED,							//so we can later call ResumeThread()
				&m_uiThreadID );							//thread id

			m_pParam = pParam;

			m_bStarted = (m_hThread != NULL);
			m_bDetached = !m_bStarted;
		}

		return m_bStarted;
	}

	void SmtThread::Suspend()
	{
		if (m_bStarted && !m_bDetached)
		{
			::SuspendThread(m_hThread);
			SetState(SMT_THREAD_SUSPENDED);
		}
	}

	ulong SmtThread::Wait(DWORD dwMilliseconds)
	{
		ulong status = (ulong) NULL;
		if (m_bStarted && !m_bDetached)
		{
			WaitForSingleObject(m_hThread, dwMilliseconds);
			GetExitCodeThread(m_hThread, &status);       
			CloseHandle(m_hThread);

			SetState(SMT_THREAD_DEAD);
			m_bDetached = true;
			m_bStarted = false;
		}

		return status;
	}

	bool SmtThread::Kill()
	{
		if (m_bStarted && !m_bDetached &&
			::TerminateThread(m_hThread,0) &&
			::CloseHandle(m_hThread))
		{
			m_hThread  = NULL;
			m_uiThreadID =0;
			m_bDetached = true;

			SetState(SMT_THREAD_TERMINATED);

			return m_bDetached;
		}
		else 
			return m_bDetached;
	}

	void SmtThread::Resume()
	{
		if (m_bStarted && !m_bDetached)
		{
			::ResumeThread(m_hThread);
			SetState(SMT_THREAD_RUNNING);
		}
	}

	void SmtThread::SetPriority(SmtThreadPriority priority)
	{
		switch(priority)
		{
		case SMT_THREAD_PRIORITY_LOWEST:
			SetThreadPriority(m_hThread,THREAD_PRIORITY_LOWEST);
			break;
		case SMT_THREAD_PRIORITY_BELOW_NORMAL:
			SetThreadPriority(m_hThread,THREAD_PRIORITY_BELOW_NORMAL);
			break;
		case SMT_THREAD_PRIORITY_NORMAL:
			SetThreadPriority(m_hThread,THREAD_PRIORITY_NORMAL);
			break;
		case SMT_THREAD_PRIORITY_ABOVE_NORMAL:
			SetThreadPriority(m_hThread,THREAD_PRIORITY_ABOVE_NORMAL);
			break;
		case SMT_THREAD_PRIORITY_HIGHEST:
			SetThreadPriority(m_hThread,THREAD_PRIORITY_HIGHEST);
			break;
		default:
		    break;
		}
		
	}
	
	SmtThreadPriority SmtThread::GetPriority()
	{
		long lPriroty = GetThreadPriority(m_hThread);

		switch(lPriroty)
		{
		case THREAD_PRIORITY_LOWEST:
			return SMT_THREAD_PRIORITY_LOWEST;
		case THREAD_PRIORITY_BELOW_NORMAL:
			return SMT_THREAD_PRIORITY_BELOW_NORMAL;
		case THREAD_PRIORITY_NORMAL:
			return SMT_THREAD_PRIORITY_NORMAL;
		case THREAD_PRIORITY_ABOVE_NORMAL:
			return SMT_THREAD_PRIORITY_ABOVE_NORMAL;
		case SMT_THREAD_PRIORITY_HIGHEST:
			return SMT_THREAD_PRIORITY_HIGHEST;
		default:
			break;
		}

		return SMT_THREAD_PRIORITY_NORMAL;
	}

	/*void SmtThread::Run()
	{
		
	}*/

	uint  __stdcall SmtThread::Proxy(void *pThread)
	{
		SmtThread *proxy = (SmtThread *)pThread;
		if (NULL != pThread)
		{
			proxy->SetState(SMT_THREAD_RUNNING);
			proxy->Run(proxy->m_pParam);
			proxy->SetState(SMT_THREAD_FINISHED);
		}
		
		return 1;
	}

	void SmtThread::Sleep(uint delay)
	{
		::Sleep(delay);
	}
}