#include "base/core/threadpool.h"

namespace base
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

	void SmtWorkThread::set_job(SmtJob* pJob,void* pJobdata)
	{
		m_VarCSLock.lock();

		m_pJob = pJob;
		m_pJobData = pJobdata;

		m_VarCSLock.unlock();

		m_JobCond.signal();
	}

	void SmtWorkThread::set_thread_pool(SmtThreadPool* pThreadPool)
	{
		m_VarCSLock.lock();
		m_pThreadPool = pThreadPool;
		m_VarCSLock.unlock();
	}

	void SmtWorkThread::wakeup(SmtJob* pJob,void* pJobdata)
	{
		m_WorkCSLock.lock();
		set_job(pJob,pJobdata);
		pJob->set_work_thread(this);
		m_WorkCSLock.unlock();

		m_JobCond.signal();
	}

	void SmtWorkThread::run(void *pParam) 
	{
		set_state(SMT_THREAD_RUNNING);
		//problem occurs here
		for(;;)
		{
			//no job in job list,so we wait.^_^
			while(m_pJob == NULL)
			{
				m_JobCond.wait(); 
			}

			m_pThreadPool->move_to_busy_list(this);	

			m_pJob->run(m_pJobData);

			SMT_SAFE_DELETE(m_pJob);
			SMT_SAFE_DELETE(m_pJobData);

			/*m_pJob->set_work_thread(NULL);
			m_pJob = NULL;*/

			m_WorkCSLock.unlock();
			
			m_pThreadPool->move_to_idle_list(this);
		}

		set_state(SMT_THREAD_FINISHED);
	}
}