/*
File:    smt_trd_sync.h

Desc:    SmartGis ,多线程同步 互斥、信号量 条件变量（内核态）

		 内核态与用户态是操作系统的两种运行级别，intel cpu提供Ring0-Ring3三种级别的运行模式。
		 Ring0级别最高，Ring3最低。其中特权级0（Ring0）是留给操作系统代码，设备驱动程序代码
		 使用的，它们工作于系统核心态；而特权极3（Ring3）则给普通的用户程序使用，它们工作在
		 用户态。运行于处理器核心态的代码不受任何的限制，可以自由地访问任何有效地址，进行直
		 接端口访问。而运行于用户态的代码则要受到处理器的诸多检查，它们只能访问映射其地址空
		 间的页表项中规定的在用户态下可访问页面的虚拟地址，且只能对任务状态段（TSS）中I/O许
		 可位图（I/O Permission Bitmap）中规定的可访问端口进行直接访问（此时处理器状态和控制
		 标志寄存器EFLAGS中的IOPL通常为0，指明当前可以进行直接I/O的最低特权级别是Ring0）。以
		 上的讨论只限于保护模式操作系统，象DOS这种模式操作系统则没有这些概念，其中的所有代码
		 都可被看作运行在核心态。
　　	 当一个任务(进程)执行系统调用而陷入内核代码中执行时，我们就称进程处于内核运行态(或简
		 称为内核态)。此时处理器处于特权级最高的(0级) 内核代码中执行。当进程处于内核态时，执
		 行的内核代码会使用当前进程的内核栈。每个进程都有自己的内核栈。当进程在执行用户自己的
		 代码时，则称其处于用户运行态(用户态)。即此时处理器在特权级最低的(3级)用户代码中运行。
  　　	 在内核态下CPU可执行任何指令，在用户态下CPU只能执行非特权指令。当CPU处于内核态，可以
		 随意进入用户态；而当CPU处于用户态时，用户从用户态切换到内核态只有在系统调用和中断两
		 种情况下发生，一般程序一开始都是运行于用户态，当程序需要使用系统资源时，就必须通过调
		 用软中断进入内核态。

Version: Version 1.0

Writter:  陈春亮

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_TRD_SYNC_H
#define _SMT_TRD_SYNC_H
#include "smt_core.h"

namespace Smt_Core
{
	//事件
	/*
	事件对象成员包括：使用计数、表示事件是否自动重置的布尔变量、事件是否触发的布尔变量
	1.当一个手动重置事件被触发的时候，正在等待的该事件的所有线程都将变成可调度的
	2.当一个自动重置事件被触发的时候，只有一个等待线程会变成可调度的

	事件最通常的用途是，让一个线程执行初始化工作，然后再触发另外一个线程，让它执行剩下的工作。
	*/
	class SMT_EXPORT_CLASS SmtEvent  
	{  
	public:  
		SmtEvent (bool bManualReset = true,bool bInitState = false,const char *szName = "")  
		{  
			if (strlen(szName) > 0)
				m_hEvent = CreateEvent(NULL, bManualReset,bInitState, szName);  
			else
				m_hEvent = CreateEvent(NULL, bManualReset,bInitState, NULL);  
		}  

		~SmtEvent (void)  
		{  
			CloseHandle (m_hEvent);  
		}  

	public:  
		//set signal
		bool	SetEvent() 
		{
			return ::SetEvent(m_hEvent) ? true : false;
		}

		//set unsignal
		bool	ResetEvent() 
		{
			return ::ResetEvent(m_hEvent) ? true : false;
		}

		//set signal, then set unsignal
		bool	PulseEvent() 
		{
			return ::PulseEvent(m_hEvent) ? true : false;
		}

		//wait event
		int		Wait(long timeout = -1)
		{
			DWORD ret = WaitForSingleObject (m_hEvent, timeout);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}

		HANDLE Handle (void)  
		{  
			return m_hEvent;  
		}  

	protected:  
		HANDLE m_hEvent; 

	};  

	//互斥量
	/*
	互斥量对象成员包括：使用计数、线程ID、递归计数
	mutex = mutual exclusive
	规则：如果线程ID为0（无效线程ID） 那么该互斥量不为任何线程所占用，它处于触发状态
		  如果线程ID为非零值		  那么有一个线程已经占用了改互斥量，它处于非触发状态
		  与其他内核对象不同，操作系统对互斥量进行了特殊处理，允许它们违反一些常用规则
	*/
	class SMT_EXPORT_CLASS SmtMutex  
	{  
	public:  
		SmtMutex (bool bInitOwner = false,const char *szName = "")  
		{  
			if (strlen(szName) > 0)
				m_hMutex = CreateMutex(NULL, bInitOwner, szName);  
			else
				m_hMutex = CreateMutex(NULL, bInitOwner, NULL);  
		}  

		~SmtMutex (void)  
		{  
			CloseHandle (m_hMutex);  
		}  

	public:  
		int Acquire (void)  
		{ //在内部等待函数会检测互斥量的线程ID
			//1.如果为0，那么函数会把互斥量的线程ID置为调用线程的线程ID，递归计数将被设为1，然后让线程
			//继续运行
			//2.如果不为0（互斥量处于未触发状态），那么调用线程会进入等待状态。当另外一个线程将互斥量
			//的线程ID设为0时，系统会记得有一个线程在等待，于是把它的线程ID设为正在等待的那个线程的线程
			//ID，把递归计数置为1，使正在等待的线程变成可调度的状态。
			/*
			假设线程试图等待一个未触发的互斥对象。在这种情况下，线程通常会进入等待状态。但是系统会检查想
			要获得互斥量的线程的线程ID是否与互斥量对象内部记录的线程ID相同。如果线程ID相同，那么系统会让
			线程保持可调度状态-即使互斥量未触发。每次线程成功地等待了一个互斥量，互斥量的递归计数会递增。
			任何成功获得对资源访问权（通过等待互斥量）的线程不在需要访问资源的时候，必须调用Release函数
			来释放互斥量。如果线程成功地等待了互斥量不止一次，那么线程必须调用Release相同的次数才能使对象
			的递归次数变成0。当递归计数变成0的同时，函数还会将线程ID设为0，这样就触发了对象
			*/
			DWORD ret = WaitForSingleObject (m_hMutex, INFINITE);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

		int Release (void)  
		{  /*
		   当线程调用该函数时，函数会检查调用线程的线程ID是否与互斥量内部保持的线程ID相同。如果相同，那么
		   递归计数会减一。如果不一致，那么将不执行任何任何操作返回-1；
		   如果在调用该函数前线程已经销毁或退出，则该互斥量被遗弃，操作系统会记录所有互斥量，因此当互斥量
		   被遗弃时，系统会将其线程ID置为0，递归计数置为0；
		   */
			BOOL bret = ReleaseMutex (m_hMutex);  

			return bret ? 0 : -1;  
		}  

		HANDLE Handle (void)  
		{  
			return m_hMutex;  
		}  

	protected:  
		HANDLE m_hMutex; 

	};  

	//信号量 
	/*
	信号量成员包括：使用计数、最大资源计数、当前可以资源计数
	规则：如果当前资源计数大于0 那么信号量处于触发状态
		  如果当前资源计数等于0 那么信号量处于非触发状态
		  系统绝对不允许当前资源计数变为负数
		  当前资源计数绝对不会大于最大资源计数
	*/
	class SMT_EXPORT_CLASS SmtSemaphore 
	{  
	public:  
		SmtSemaphore (long init_count/*初始可供使用资源个数*/, 
					  long max_count = (std::numeric_limits<long>::max)()/*最大可供使用资源个数*/,
					  const char *szName = "")  
		{  
			assert (init_count >= 0 && max_count > 0 && init_count <= max_count); 

			if (strlen(szName) > 0)
				m_hSemaphore = CreateSemaphore(NULL, init_count, max_count, szName);  
			else
				m_hSemaphore = CreateSemaphore(NULL, init_count, max_count, NULL);   
		}  

		~SmtSemaphore (void)  
		{  
			CloseHandle (m_hSemaphore);  
		}  

	public:  
		int Post (long count = 1)  
		{//增加当前资源计数 增加count 个 
			BOOL bret = ReleaseSemaphore (m_hSemaphore, count, NULL);  
			return bret ? 0 : -1;  
		}  

		int Wait (long timeout = -1)  
		{//等待信号量,在内部等待函数会检查信号量当前资源计数。
			//1.如果它的值大于0（信号量处于触发状态），那么函数会把计数器减1并让调用线程继续运行。
			//即操作系统会检测资源是否可用，并将可用资源数量递减，整个过程线程不会被别的线程打断。
			//只有当资源计数递减完成之后，系统才会允许另外一个线程请求对资源的访问
			//2.如果它的值等于0，那么系统会让调用线程进入等待状态。当另外一个线程将信号量的当前资
			//源计数递增时，系统会将那个还在等待的线程变成可调度状态
			DWORD ret = WaitForSingleObject (m_hSemaphore, timeout);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

		HANDLE Handle (void)  
		{  
			return m_hSemaphore;  
		}  

	protected:  
		HANDLE m_hSemaphore;  

	};  

	class SMT_EXPORT_CLASS SmtCondition  
	{  
	public:  
		SmtCondition (void)  
			: m_lWaiters (0)  
			, m_semaphore (0)  
		{  
		}  

		~SmtCondition (void)  
		{  
		}  

	public:  
		/// Returns a reference to the underlying m_cslock;  
		SmtMutex &Mutex (void)  
		{  
			return m_mutex;  
		}  

		/// Signal one waiting thread.  
		int Signal (void)  
		{ //触发 
			// must hold the external mutex before enter  
			if ( m_lWaiters > 0 )  
				m_semaphore.Post ();  

			return 0;  
		}  

		/// Signal *all* waiting threads.  
		int Broadcast (void)  
		{  //触发 
			// must hold the external mutex before enter  
			if ( m_lWaiters > 0 )  
				m_semaphore.Post (m_lWaiters);  

			return 0;  
		}  

		int Wait (unsigned long wait_time = -1)  
		{  
			// must hold the external mutex before enter  
			int ret = 0;  
			m_lWaiters++;  

			ret = SignalObjectAndWait (m_mutex.Handle (),
				m_semaphore.Handle (), wait_time, FALSE); 

			m_mutex.Acquire();  

			m_lWaiters --;  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

	protected:  
		SmtMutex		m_mutex;  

		long			m_lWaiters;		// Number of waiting threads.  
		SmtSemaphore	m_semaphore;	// Queue up threads waiting for the condition to become signaled. 
	};  
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_TRD_SYNC_H