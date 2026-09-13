/*
File:    smt_threadpool.h

Desc:    SmartGis ,�̳߳�

Version: Version 1.0

Writter:  �´���

Date:    2012.7.23

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_THREADPOOL_H
#define _SMT_THREADPOOL_H

#include "base/core/core.h"
#include "base/core/thread.h"
#include "base/core/cslock.h"
#include "base/core/trd_sync.h"

#define					SMT_JOBNAME_LENGTH			MAX_NAME_LENGTH
namespace base
{
	class SmtThreadPool;
	class CORE_EXPORT SmtJob 
	{ 
	public: 
		SmtJob( void ):m_pWorkThread(NULL),m_nJobNo(-1){m_szJobName[0] = '\0';} 
		virtual ~SmtJob(){m_pWorkThread = NULL;}    

	public: 
		int						get_job_no(void) const { return m_nJobNo; } 
		void					set_job_no(int jobno){ m_nJobNo = jobno;} 
		const char*				get_job_name(void) const { return m_szJobName; } 
		void					set_job_name(const char* jobname) {if (strlen(jobname)>0)strcpy_s(m_szJobName,SMT_JOBNAME_LENGTH,jobname);}

		const SmtThread			*get_work_thread(void) const{ return m_pWorkThread; } 
		SmtThread				*get_work_thread(void) { return m_pWorkThread; } 
		void					set_work_thread ( SmtThread *pWorkThread ){ m_pWorkThread = pWorkThread;} 

		virtual void			run ( void *ptr ) = 0; 

	protected: 
		int						m_nJobNo;									//The num was assigned to the job 
		char					m_szJobName[SMT_JOBNAME_LENGTH];			//The job name 
		SmtThread				*m_pWorkThread;								//The thread associated with the job 
	}; 

	class CORE_EXPORT SmtWorkThread:public SmtThread
	{
		friend class SmtThreadPool;
	public:
		SmtWorkThread();
		virtual ~SmtWorkThread();

	public:
		void					set_job(SmtJob* pJob,void* pJobdata); 
		SmtJob*					get_job(void){return m_pJob;}

		void					set_thread_pool(SmtThreadPool* pThreadPool);
		SmtThreadPool*			get_thread_pool(void){return m_pThreadPool;}
		bool					is_working(void){return m_pJob != NULL;} 

		void					wakeup(SmtJob* pJob,void* pJobdata);

	public:
		virtual void			run(void *pParam) ;

	protected:
		SmtThreadPool*			m_pThreadPool;

		SmtJob*					m_pJob;					//the job that associate with the workthread
		void*					m_pJobData;	

		SmtCSLock				m_VarCSLock;
		bool 					m_IsEnd;

		SmtCondition			m_JobCond;
		SmtCSLock				m_WorkCSLock;
	};

	class CORE_EXPORT SmtThreadPool
	{
	public:
		SmtThreadPool();
		SmtThreadPool(int nMaxNum,int nAvailIdleLow,int nAvailIdleHigh,int nNormalIdleNum); 
		virtual ~SmtThreadPool();  

	public:
		SmtWorkThread			*get_idle_thread(void);   
		void					append_to_idle_list(SmtWorkThread* pJobThread); 
		void					move_to_busy_list(SmtWorkThread* pIdleThread); 
		void					move_to_idle_list(SmtWorkThread* pBusyThread);

		void					create_thread(int nNum); 
		void					delete_thread(int nNum); 

		inline void				set_max_num(int maxnum){m_unMaxNum = maxnum;} 
		inline int				get_max_num(void){return m_unMaxNum;} 

		inline void				set_avail_low_num(int minnum){m_unAvailIdleLow = minnum;} 
		inline int				get_avail_low_num(void){return m_unAvailIdleLow;} 

		inline void				set_avail_high_num(int highnum){m_unAvailIdleHigh = highnum;} 
		inline int				get_avail_high_num(void){return m_unAvailIdleHigh;} 

		inline int				get_actual_avail_num(void){return m_unIdleNum;} 
		inline int				get_all_num(void){return m_vThreads.size();}

		inline int				get_idle_num(void){return m_unIdleNum/*m_vIdleThreads.size()*/;} 
		inline int				get_busy_num(void){return m_unBusyNum/*m_vBusyThreads.size()*/;} 

		inline void				set_normal_num(int unNormalNum ){m_unNormalIdleNum = unNormalNum ;} 
		inline int				get_normal_num(void){return m_unNormalIdleNum;} 

		void					terminate_all(void); 

		void					run(SmtJob* job,void* jobdata); 

	protected:
		SmtCSLock				m_BusyCSLock;		//when visit busy list,use m_BusyCSLock to lock and unlock 
		SmtCSLock				m_IdleCSLock;		//when visit idle list,use m_IdleCSLock to lock and unlock 
		SmtCSLock				m_JobCSLock;			//when visit job list,use m_JobCSLock to lock and unlock 
		SmtCSLock				m_VarCSLock; 

		SmtCondition			m_BusyCond;			//m_BusyCond is used to sync busy thread list 
		SmtCondition			m_IdleCond;			//m_IdleCond is used to sync idle thread list 
		SmtCondition			m_IdleJobCond;		//m_JobCond is used to sync job list 
		SmtCondition			m_MaxNumCond;

		unsigned int			m_unMaxNum;			//the max thread num that can create at the same time 
		unsigned int			m_unAvailIdleLow;	//The min num of idle thread that shoule kept 
		unsigned int			m_unAvailIdleHigh;	//The max num of idle thread that kept at the same time 
		unsigned int			m_unIdleNum;		//the  num of idle thread; 
		unsigned int			m_unBusyNum;		//the  num of busy thread; 
		unsigned int			m_unNormalIdleNum;	//Normal idle thread num; 

		vector<SmtWorkThread*>  m_vThreads; 
		vector<SmtWorkThread*>  m_vBusyThreads; //Thread List 
		vector<SmtWorkThread*>  m_vIdleThreads; //Idle List 
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_THREADPOOL_H
