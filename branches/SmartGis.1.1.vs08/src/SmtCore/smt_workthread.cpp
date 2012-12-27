#include "smt_threadpool.h"

namespace Smt_Core
{
	SmtWorkThread::SmtWorkThread()
	{
		m_pJob = NULL;
		m_pJobData = NULL;
		m_pThreadPool = NULL;
		m_IsEnd = false;
	}

	SmtWorkThread:: ~SmtWorkThread()
	{
		m_pJob = NULL;
		m_pJobData = NULL;
		m_pThreadPool = NULL;
	}

	void SmtWorkThread::SetJob(SmtJob* pJob,void* pJobdata)
	{
		m_VarCSLock.Lock();

		m_pJob = pJob;
		m_pJobData = pJobdata;

		m_VarCSLock.Unlock();

		m_JobCond.Signal();
	}

	void SmtWorkThread::SetThreadPool(SmtThreadPool* pThreadPool)
	{
		m_VarCSLock.Lock();
		m_pThreadPool = pThreadPool;
		m_VarCSLock.Unlock();
	}

	void SmtWorkThread::Wakeup(SmtJob* pJob,void* pJobdata)
	{
		m_WorkCSLock.Lock();
		SetJob(pJob,pJobdata);
		pJob->SetWorkThread(this);
		m_WorkCSLock.Unlock();

		m_JobCond.Signal();
	}

	void SmtWorkThread::Run(void *pParam) 
	{
		SetState(SMT_THREAD_RUNNING);
		//problem occurs here
		for(;;)
		{
			//no job in job list,so we wait.^_^
			while(m_pJob == NULL)
			{
				m_JobCond.Wait(); 
			}

			m_pThreadPool->MoveToBusyList(this);	

			m_pJob->Run(m_pJobData);

			SMT_SAFE_DELETE(m_pJob);
			SMT_SAFE_DELETE(m_pJobData);

			/*m_pJob->SetWorkThread(NULL);
			m_pJob = NULL;*/

			m_WorkCSLock.Unlock();
			
			m_pThreadPool->MoveToIdleList(this);
		}

		SetState(SMT_THREAD_FINISHED);
	}
}