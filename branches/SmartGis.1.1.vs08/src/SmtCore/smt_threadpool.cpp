#include "smt_threadpool.h"
#include <algorithm>
/*
m_unNormalIdleNum：		初始创建时线程池中的线程的个数。 
m_MaxNum:		当前线程池中所允许并发存在的线程的最大数目。 
m_AvailIdleLow:		当前线程池中所允许存在的空闲线程的最小数目，如果空闲数目低于该值，表明负载可能过重，此时有必要增加空闲线程池的数目。
m_AvailIdleHigh：	当前线程池中所允许的空闲的线程的最大数目，如果空闲数目高于该值，表明当前负载可能较轻，此时将删除多余的空闲线程。
m_AvailNum：	目前线程池中实际存在的线程的个数，其值介于m_AvailHigh和m_AvailLow之间。如果线程的个数始终维 持在m_AvailLow和m_AvailHigh之间，则线程既不需要创建，也不需要删除，
				保持平衡状态。因此如何设定m_AvailLow和 m_AvailHigh的值，使得线程池最大可能的保持平衡态，是线程池设计必须考虑的问题。
*/
namespace Smt_Core
{
	SmtThreadPool::SmtThreadPool():m_unNormalIdleNum(20)
		,m_unMaxNum(50)
		,m_unAvailIdleLow(5)
		,m_unAvailIdleHigh(30)
		,m_unIdleNum(0)
		,m_unBusyNum(0)
	{
		m_vIdleThreads.clear(); 
		m_vBusyThreads.clear(); 

		CreateThread(m_unNormalIdleNum);
	}

	SmtThreadPool::SmtThreadPool(int nMaxNum,int nAvailIdleLow,int nAvailIdleHigh,int nNormalIdleNum)
	{
		assert(nMaxNum > 0);
		assert(nAvailIdleLow > 0 && nAvailIdleLow <= nMaxNum);
		assert(nAvailIdleHigh > 0 && nAvailIdleHigh <= nMaxNum);
		assert(nAvailIdleHigh > nAvailIdleLow);
		assert(nNormalIdleNum < nAvailIdleHigh);
		assert(nNormalIdleNum > nAvailIdleLow);
		assert(nNormalIdleNum > 0 && nNormalIdleNum <=nMaxNum);

		m_unMaxNum = nMaxNum;
		m_unAvailIdleLow = nAvailIdleLow;
		m_unAvailIdleHigh = nAvailIdleHigh; 

		m_unIdleNum = 0;
		m_unBusyNum = 0;
		m_unNormalIdleNum = nNormalIdleNum;

		m_vIdleThreads.clear(); 
		m_vBusyThreads.clear(); 

		CreateThread(m_unNormalIdleNum);
	}

	SmtThreadPool::~SmtThreadPool()  
	{
		  TerminateAll(); 

		  for(int i=0;i < m_vThreads.size();i++) 
		  {
			  SmtWorkThread* pThread = m_vThreads[i];
			  SMT_SAFE_DELETE(pThread);
		  }

		  m_vThreads.clear();
		  m_vIdleThreads.clear(); 
		  m_vBusyThreads.clear(); 
	}

	SmtWorkThread* SmtThreadPool::GetIdleThread(void)   
	{
		if(m_vIdleThreads.size() ==0 )
			m_IdleCond.Wait();

		m_IdleCSLock.Lock(); 

		if(m_vIdleThreads.size() > 0 ) 
		{ 
			SmtWorkThread* pThread = (SmtWorkThread*)m_vIdleThreads.front(); 

			m_IdleCSLock.Unlock(); 

			return pThread; 
		} 

		m_IdleCSLock.Unlock(); 

		return NULL;
	}

	void SmtThreadPool::AppendToIdleList(SmtWorkThread* pJobThread) 
	{
		m_IdleCSLock.Lock(); 
		
		m_vIdleThreads.push_back(pJobThread);

		m_unIdleNum++; 

		m_IdleCSLock.Unlock(); 
	}

	void SmtThreadPool::MoveToBusyList(SmtWorkThread* pIdleThread) 
	{
		m_BusyCSLock.Lock(); 

		m_vBusyThreads.push_back(pIdleThread); 
		
		m_unBusyNum++;

		m_BusyCSLock.Unlock(); 


		m_IdleCSLock.Lock(); 

		vector<SmtWorkThread*>::iterator iter; 
		iter = find(m_vIdleThreads.begin(),m_vIdleThreads.end(),pIdleThread); 

		if(iter !=m_vIdleThreads.end()) 
		{
			m_vIdleThreads.erase(iter); 
			m_unIdleNum--;
		}

		m_IdleCSLock.Unlock(); 
	}

	void SmtThreadPool::MoveToIdleList(SmtWorkThread* pBusyThread)
	{
		m_IdleCSLock.Lock(); 

		m_vIdleThreads.push_back(pBusyThread); 
		m_unIdleNum++; 

		m_IdleCSLock.Unlock(); 


		m_BusyCSLock.Lock(); 

		vector<SmtWorkThread*>::iterator iter; 
		iter = find(m_vBusyThreads.begin(),m_vBusyThreads.end(),pBusyThread); 

		if(iter!=m_vBusyThreads.end()) 
		{
			m_vBusyThreads.erase(iter); 
			m_unBusyNum--;
		}

		m_BusyCSLock.Unlock(); 

		m_IdleCond.Signal(); 
		m_MaxNumCond.Signal(); 
	}

	void SmtThreadPool::CreateThread(int nNum)
	{
		for(int i=0;i<nNum;i++)
		{ 
			SmtWorkThread* pThread = new SmtWorkThread(); 
			m_vThreads.push_back(pThread);

			AppendToIdleList(pThread);

			pThread->SetThreadPool(this); 

			pThread->Start();       //begin the thread,the thread wait for job 
			pThread->Resume();
			//pThread->Suspend();
		}
	}

	void SmtThreadPool::DeleteThread(int nNum)
	{
		for(int i=0 ; i <nNum ; i++)
		{ 
			SmtWorkThread* pThread = NULL; 

			if(m_vIdleThreads.size() > 0 )
			{ 
				pThread = (SmtWorkThread*)m_vIdleThreads.front(); 
			} 

			vector<SmtWorkThread*>::iterator iterIdle; 
			vector<SmtWorkThread*>::iterator iter; 

			iterIdle = find(m_vIdleThreads.begin(),m_vIdleThreads.end(),pThread); 
			iter = find(m_vThreads.begin(),m_vThreads.end(),pThread); 

			if(iterIdle!=m_vIdleThreads.end() && iter!=m_vThreads.end() ) 
			{
				m_IdleCSLock.Lock(); 

				m_vIdleThreads.erase(iterIdle); 
				m_vThreads.erase(iterIdle); 
				SMT_SAFE_DELETE(pThread);

				m_unIdleNum--; 

				m_IdleCSLock.Unlock(); 
			}
		} 	
	}

	void SmtThreadPool::TerminateAll(void) 
	{
		for(int i=0;i < m_vThreads.size();i++) 
		{
			SmtWorkThread* pThread = m_vThreads[i];
			pThread->Kill();
		}
	}

	void SmtThreadPool::Run(SmtJob* pJob,void* pJobdata)
	{
		assert(pJob!=NULL); 
	
		if(GetBusyNum() == m_unMaxNum) 
			m_MaxNumCond.Wait(); 

		if(m_vIdleThreads.size()<m_unAvailIdleLow) 
		{ //负载过重
			if(GetAllNum()+m_unNormalIdleNum-m_vIdleThreads.size() < m_unMaxNum ) 
				CreateThread(m_unNormalIdleNum-m_vIdleThreads.size()); 
			else 
				CreateThread(m_unMaxNum-GetAllNum()); 
		}
		else if (m_vIdleThreads.size() > m_unAvailIdleHigh)
		{//负载过轻
			DeleteThread(m_vIdleThreads.size() - m_unNormalIdleNum); 
		}

		SmtWorkThread*  pIdleThread = GetIdleThread(); 

		if(pIdleThread !=NULL) 
		{ 
			pIdleThread->m_WorkCSLock.Lock(); 

			MoveToBusyList(pIdleThread); 

			pIdleThread->SetThreadPool(this); 

			pJob->SetWorkThread(pIdleThread); 
			pIdleThread->SetJob(pJob,pJobdata); 
		} 
	}
}