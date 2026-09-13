#include "base/core/threadpool.h"
#include <algorithm>
/*
m_unNormalIdleNum��		��ʼ����ʱ�̳߳��е��̵߳ĸ����� 
m_MaxNum:		��ǰ�̳߳����������������ڵ��̵߳������Ŀ�� 
m_AvailIdleLow:		��ǰ�̳߳������������ڵĿ����̵߳���С��Ŀ�����������Ŀ���ڸ�ֵ���������ؿ��ܹ��أ���ʱ�б�Ҫ���ӿ����̳߳ص���Ŀ��
m_AvailIdleHigh��	��ǰ�̳߳����������Ŀ��е��̵߳������Ŀ�����������Ŀ���ڸ�ֵ��������ǰ���ؿ��ܽ��ᣬ��ʱ��ɾ������Ŀ����̡߳�
m_AvailNum��	Ŀǰ�̳߳���ʵ�ʴ��ڵ��̵߳ĸ�������ֵ����m_AvailHigh��m_AvailLow֮�䡣����̵߳ĸ���ʼ��ά ����m_AvailLow��m_AvailHigh֮�䣬���̼߳Ȳ���Ҫ������Ҳ����Ҫɾ����
				����ƽ��״̬���������趨m_AvailLow�� m_AvailHigh��ֵ��ʹ���̳߳������ܵı���ƽ��̬�����̳߳���Ʊ��뿼�ǵ����⡣
*/
namespace base
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

		create_thread(m_unNormalIdleNum);
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

		create_thread(m_unNormalIdleNum);
	}

	SmtThreadPool::~SmtThreadPool()  
	{
		  terminate_all(); 

		  for(int i=0;i < m_vThreads.size();i++) 
		  {
			  SmtWorkThread* pThread = m_vThreads[i];
			  SMT_SAFE_DELETE(pThread);
		  }

		  m_vThreads.clear();
		  m_vIdleThreads.clear(); 
		  m_vBusyThreads.clear(); 
	}

	SmtWorkThread* SmtThreadPool::get_idle_thread(void)   
	{
		if(m_vIdleThreads.size() ==0 )
			m_IdleCond.wait();

		m_IdleCSLock.lock(); 

		if(m_vIdleThreads.size() > 0 ) 
		{ 
			SmtWorkThread* pThread = (SmtWorkThread*)m_vIdleThreads.front(); 

			m_IdleCSLock.unlock(); 

			return pThread; 
		} 

		m_IdleCSLock.unlock(); 

		return NULL;
	}

	void SmtThreadPool::append_to_idle_list(SmtWorkThread* pJobThread) 
	{
		m_IdleCSLock.lock(); 
		
		m_vIdleThreads.push_back(pJobThread);

		m_unIdleNum++; 

		m_IdleCSLock.unlock(); 
	}

	void SmtThreadPool::move_to_busy_list(SmtWorkThread* pIdleThread) 
	{
		m_BusyCSLock.lock(); 

		m_vBusyThreads.push_back(pIdleThread); 
		
		m_unBusyNum++;

		m_BusyCSLock.unlock(); 


		m_IdleCSLock.lock(); 

		vector<SmtWorkThread*>::iterator iter; 
		iter = find(m_vIdleThreads.begin(),m_vIdleThreads.end(),pIdleThread); 

		if(iter !=m_vIdleThreads.end()) 
		{
			m_vIdleThreads.erase(iter); 
			m_unIdleNum--;
		}

		m_IdleCSLock.unlock(); 
	}

	void SmtThreadPool::move_to_idle_list(SmtWorkThread* pBusyThread)
	{
		m_IdleCSLock.lock(); 

		m_vIdleThreads.push_back(pBusyThread); 
		m_unIdleNum++; 

		m_IdleCSLock.unlock(); 


		m_BusyCSLock.lock(); 

		vector<SmtWorkThread*>::iterator iter; 
		iter = find(m_vBusyThreads.begin(),m_vBusyThreads.end(),pBusyThread); 

		if(iter!=m_vBusyThreads.end()) 
		{
			m_vBusyThreads.erase(iter); 
			m_unBusyNum--;
		}

		m_BusyCSLock.unlock(); 

		m_IdleCond.signal(); 
		m_MaxNumCond.signal(); 
	}

	void SmtThreadPool::create_thread(int nNum)
	{
		for(int i=0;i<nNum;i++)
		{ 
			SmtWorkThread* pThread = new SmtWorkThread(); 
			m_vThreads.push_back(pThread);

			append_to_idle_list(pThread);

			pThread->set_thread_pool(this); 

			pThread->start();       //begin the thread,the thread wait for job 
			pThread->resume();
			//pThread->Suspend();
		}
	}

	void SmtThreadPool::delete_thread(int nNum)
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
				m_IdleCSLock.lock(); 

				m_vIdleThreads.erase(iterIdle); 
				m_vThreads.erase(iterIdle); 
				SMT_SAFE_DELETE(pThread);

				m_unIdleNum--; 

				m_IdleCSLock.unlock(); 
			}
		} 	
	}

	void SmtThreadPool::terminate_all(void) 
	{
		for(int i=0;i < m_vThreads.size();i++) 
		{
			SmtWorkThread* pThread = m_vThreads[i];
			pThread->kill();
		}
	}

	void SmtThreadPool::run(SmtJob* pJob,void* pJobdata)
	{
		assert(pJob!=NULL); 
	
		if(get_busy_num() == m_unMaxNum) 
			m_MaxNumCond.wait(); 

		if(m_vIdleThreads.size()<m_unAvailIdleLow) 
		{ //���ع���
			if(get_all_num()+m_unNormalIdleNum-m_vIdleThreads.size() < m_unMaxNum ) 
				create_thread(m_unNormalIdleNum-m_vIdleThreads.size()); 
			else 
				create_thread(m_unMaxNum-get_all_num()); 
		}
		else if (m_vIdleThreads.size() > m_unAvailIdleHigh)
		{//���ع���
			delete_thread(m_vIdleThreads.size() - m_unNormalIdleNum); 
		}

		SmtWorkThread*  pIdleThread = get_idle_thread(); 

		if(pIdleThread !=NULL) 
		{ 
			pIdleThread->m_WorkCSLock.lock(); 

			move_to_busy_list(pIdleThread); 

			pIdleThread->set_thread_pool(this); 

			pJob->set_work_thread(pIdleThread); 
			pIdleThread->set_job(pJob,pJobdata); 
		} 
	}
}