/*
File:    smt_threadpool.h

Desc:    SmartGis ,Ïß³Ì³Ø

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.7.23

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_THREADPOOL_H
#define _SMT_THREADPOOL_H

#include "smt_core.h"
#include "smt_thread.h"
#include "smt_cslock.h"
#include "smt_trd_sync.h"

#define					SMT_JOBNAME_LENGTH			MAX_NAME_LENGTH
namespace Smt_Core
{
	class SmtThreadPool;
	class SMT_EXPORT_CLASS SmtJob 
	{ 
	public: 
		SmtJob( void ):m_pWorkThread(NULL),m_nJobNo(-1){m_szJobName[0] = '\0';} 
		virtual ~SmtJob(){m_pWorkThread = NULL;}    

	public: 
		int						GetJobNo(void) const { return m_nJobNo; } 
		void					SetJobNo(int jobno){ m_nJobNo = jobno;} 
		const char*				GetJobName(void) const { return m_szJobName; } 
		void					SetJobName(const char* jobname) {if (strlen(jobname)>0)strcpy_s(m_szJobName,SMT_JOBNAME_LENGTH,jobname);}

		const SmtThread			*GetWorkThread(void) const{ return m_pWorkThread; } 
		SmtThread				*GetWorkThread(void) { return m_pWorkThread; } 
		void					SetWorkThread ( SmtThread *pWorkThread ){ m_pWorkThread = pWorkThread;} 

		virtual void			Run ( void *ptr ) = 0; 

	protected: 
		int						m_nJobNo;									//The num was assigned to the job 
		char					m_szJobName[SMT_JOBNAME_LENGTH];			//The job name 
		SmtThread				*m_pWorkThread;								//The thread associated with the job 
	}; 

	class SMT_EXPORT_CLASS SmtWorkThread:public SmtThread
	{
		friend class SmtThreadPool;
	public:
		SmtWorkThread();
		virtual ~SmtWorkThread();

	public:
		void					SetJob(SmtJob* pJob,void* pJobdata); 
		SmtJob*					GetJob(void){return m_pJob;}

		void					SetThreadPool(SmtThreadPool* pThreadPool);
		SmtThreadPool*			GetThreadPool(void){return m_pThreadPool;}
		bool					IsWorking(void){return m_pJob != NULL;} 

		void					Wakeup(SmtJob* pJob,void* pJobdata);

	public:
		virtual void			Run(void *pParam) ;

	protected:
		SmtThreadPool*			m_pThreadPool;

		SmtJob*					m_pJob;					//the job that associate with the workthread
		void*					m_pJobData;	

		SmtCSLock				m_VarCSLock;
		bool 					m_IsEnd;

		SmtCondition			m_JobCond;
		SmtCSLock				m_WorkCSLock;
	};

	class SMT_EXPORT_CLASS SmtThreadPool
	{
	public:
		SmtThreadPool();
		SmtThreadPool(int nMaxNum,int nAvailIdleLow,int nAvailIdleHigh,int nNormalIdleNum); 
		virtual ~SmtThreadPool();  

	public:
		SmtWorkThread			*GetIdleThread(void);   
		void					AppendToIdleList(SmtWorkThread* pJobThread); 
		void					MoveToBusyList(SmtWorkThread* pIdleThread); 
		void					MoveToIdleList(SmtWorkThread* pBusyThread);

		void					CreateThread(int nNum); 
		void					DeleteThread(int nNum); 

		inline void				SetMaxNum(int maxnum){m_unMaxNum = maxnum;} 
		inline int				GetMaxNum(void){return m_unMaxNum;} 

		inline void				SetAvailLowNum(int minnum){m_unAvailIdleLow = minnum;} 
		inline int				GetAvailLowNum(void){return m_unAvailIdleLow;} 

		inline void				SetAvailHighNum(int highnum){m_unAvailIdleHigh = highnum;} 
		inline int				GetAvailHighNum(void){return m_unAvailIdleHigh;} 

		inline int				GetActualAvailNum(void){return m_unIdleNum;} 
		inline int				GetAllNum(void){return m_vThreads.size();}

		inline int				GetIdleNum(void){return m_unIdleNum/*m_vIdleThreads.size()*/;} 
		inline int				GetBusyNum(void){return m_unBusyNum/*m_vBusyThreads.size()*/;} 

		inline void				SetNormalNum(int unNormalNum ){m_unNormalIdleNum = unNormalNum ;} 
		inline int				GetNormalNum(void){return m_unNormalIdleNum;} 

		void					TerminateAll(void); 

		void					Run(SmtJob* job,void* jobdata); 

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

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_THREADPOOL_H
