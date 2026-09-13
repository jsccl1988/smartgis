/*
File:    smt_trd_sync.h

Desc:    SmartGis ,���߳�ͬ�� ���⡢�ź��� �����������ں�̬��

		 �ں�̬���û�̬�ǲ���ϵͳ���������м���intel cpu�ṩRing0-Ring3���ּ��������ģʽ��
		 Ring0������ߣ�Ring3��͡�������Ȩ��0��Ring0������������ϵͳ���룬�豸�����������
		 ʹ�õģ����ǹ�����ϵͳ����̬������Ȩ��3��Ring3�������ͨ���û�����ʹ�ã����ǹ�����
		 �û�̬�������ڴ���������̬�Ĵ��벻���κε����ƣ��������ɵط����κ���Ч��ַ������ֱ
		 �Ӷ˿ڷ��ʡ����������û�̬�Ĵ�����Ҫ�ܵ�������������飬����ֻ�ܷ���ӳ�����ַ��
		 ���ҳ�����й涨�����û�̬�¿ɷ���ҳ��������ַ����ֻ�ܶ�����״̬�Σ�TSS����I/O��
		 ��λͼ��I/O Permission Bitmap���й涨�Ŀɷ��ʶ˿ڽ���ֱ�ӷ��ʣ���ʱ������״̬�Ϳ���
		 ��־�Ĵ���EFLAGS�е�IOPLͨ��Ϊ0��ָ����ǰ���Խ���ֱ��I/O�������Ȩ������Ring0������
		 �ϵ�����ֻ���ڱ���ģʽ����ϵͳ����DOS����ģʽ����ϵͳ��û����Щ������е����д���
		 ���ɱ����������ں���̬��
����	 ��һ������(����)ִ��ϵͳ���ö������ں˴�����ִ��ʱ�����Ǿͳƽ��̴����ں�����̬(���
		 ��Ϊ�ں�̬)����ʱ������������Ȩ����ߵ�(0��) �ں˴�����ִ�С������̴����ں�̬ʱ��ִ
		 �е��ں˴����ʹ�õ�ǰ���̵��ں�ջ��ÿ�����̶����Լ����ں�ջ����������ִ���û��Լ���
		 ����ʱ������䴦���û�����̬(�û�̬)������ʱ����������Ȩ����͵�(3��)�û����������С�
  ����	 ���ں�̬��CPU��ִ���κ�ָ����û�̬��CPUֻ��ִ�з���Ȩָ���CPU�����ں�̬������
		 ��������û�̬������CPU�����û�̬ʱ���û����û�̬�л����ں�ֻ̬����ϵͳ���ú��ж���
		 ������·�����һ�����һ��ʼ�����������û�̬����������Ҫʹ��ϵͳ��Դʱ���ͱ���ͨ����
		 �����жϽ����ں�̬��

Version: Version 1.0

Writter:  �´���

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_TRD_SYNC_H
#define _SMT_TRD_SYNC_H
#include "base/core/core.h"

namespace base
{
	//�¼�
	/*
	�¼������Ա������ʹ�ü�������ʾ�¼��Ƿ��Զ����õĲ����������¼��Ƿ񴥷��Ĳ�������
	1.��һ���ֶ������¼���������ʱ�����ڵȴ��ĸ��¼��������̶߳�����ɿɵ��ȵ�
	2.��һ���Զ������¼���������ʱ��ֻ��һ���ȴ��̻߳��ɿɵ��ȵ�

	�¼���ͨ������;�ǣ���һ���߳�ִ�г�ʼ��������Ȼ���ٴ�������һ���̣߳�����ִ��ʣ�µĹ�����
	*/
	class CORE_EXPORT SmtEvent  
	{  
	public:  
		SmtEvent (bool bManualReset = true,bool bInitState = false,const char *szName = "")  
		{  
			if (strlen(szName) > 0)
				m_hEvent = CreateEventA(NULL, bManualReset,bInitState, szName);  
			else
				m_hEvent = CreateEventA(NULL, bManualReset,bInitState, NULL);  
		}  

		~SmtEvent (void)  
		{  
			CloseHandle (m_hEvent);  
		}  

	public:  
		//set signal
		bool	set_event() 
		{
			return ::SetEvent(m_hEvent) ? true : false;
		}

		//set unsignal
		bool	reset_event() 
		{
			return ::ResetEvent(m_hEvent) ? true : false;
		}

		//set signal, then set unsignal
		bool	pulse_event() 
		{
			return ::PulseEvent(m_hEvent) ? true : false;
		}

		//wait event
		int		wait(long timeout = -1)
		{
			DWORD ret = WaitForSingleObject (m_hEvent, timeout);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}

		HANDLE handle (void)  
		{  
			return m_hEvent;  
		}  

	protected:  
		HANDLE m_hEvent; 

	};  

	//������
	/*
	�����������Ա������ʹ�ü������߳�ID���ݹ����
	mutex = mutual exclusive
	��������߳�IDΪ0����Ч�߳�ID�� ��ô�û�������Ϊ�κ��߳���ռ�ã������ڴ���״̬
		  ����߳�IDΪ����ֵ		  ��ô��һ���߳��Ѿ�ռ���˸Ļ������������ڷǴ���״̬
		  �������ں˶���ͬ������ϵͳ�Ի��������������⴦������������Υ��һЩ���ù���
	*/
	class CORE_EXPORT SmtMutex  
	{  
	public:  
		SmtMutex (bool bInitOwner = false,const char *szName = "")  
		{  
			if (strlen(szName) > 0)
				m_hMutex = CreateMutexA(NULL, bInitOwner, szName);  
			else
				m_hMutex = CreateMutexA(NULL, bInitOwner, NULL);  
		}  

		~SmtMutex (void)  
		{  
			CloseHandle (m_hMutex);  
		}  

	public:  
		int acquire (void)  
		{ //���ڲ��ȴ��������⻥�������߳�ID
			//1.���Ϊ0����ô������ѻ��������߳�ID��Ϊ�����̵߳��߳�ID���ݹ����������Ϊ1��Ȼ�����߳�
			//��������
			//2.�����Ϊ0������������δ����״̬������ô�����̻߳����ȴ�״̬��������һ���߳̽�������
			//���߳�ID��Ϊ0ʱ��ϵͳ��ǵ���һ���߳��ڵȴ������ǰ������߳�ID��Ϊ���ڵȴ����Ǹ��̵߳��߳�
			//ID���ѵݹ������Ϊ1��ʹ���ڵȴ����̱߳�ɿɵ��ȵ�״̬��
			/*
			�����߳���ͼ�ȴ�һ��δ�����Ļ����������������£��߳�ͨ�������ȴ�״̬������ϵͳ������
			Ҫ��û��������̵߳��߳�ID�Ƿ��뻥���������ڲ���¼���߳�ID��ͬ������߳�ID��ͬ����ôϵͳ����
			�̱߳��ֿɵ���״̬-��ʹ������δ������ÿ���̳߳ɹ��صȴ���һ�����������������ĵݹ�����������
			�κγɹ���ö���Դ����Ȩ��ͨ���ȴ������������̲߳�����Ҫ������Դ��ʱ�򣬱������Release����
			���ͷŻ�����������̳߳ɹ��صȴ��˻�������ֹһ�Σ���ô�̱߳������Release��ͬ�Ĵ�������ʹ����
			�ĵݹ�������0�����ݹ�������0��ͬʱ���������Ὣ�߳�ID��Ϊ0�������ʹ����˶���
			*/
			DWORD ret = WaitForSingleObject (m_hMutex, INFINITE);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

		int release (void)  
		{  /*
		   ���̵߳��øú���ʱ��������������̵߳��߳�ID�Ƿ��뻥�����ڲ����ֵ��߳�ID��ͬ�������ͬ����ô
		   �ݹ�������һ�������һ�£���ô����ִ���κ��κβ�������-1��
		   ����ڵ��øú���ǰ�߳��Ѿ����ٻ��˳�����û�����������������ϵͳ���¼���л���������˵�������
		   ������ʱ��ϵͳ�Ὣ���߳�ID��Ϊ0���ݹ������Ϊ0��
		   */
			BOOL bret = ReleaseMutex (m_hMutex);  

			return bret ? 0 : -1;  
		}  

		HANDLE handle (void)  
		{  
			return m_hMutex;  
		}  

	protected:  
		HANDLE m_hMutex; 

	};  

	//�ź��� 
	/*
	�ź�����Ա������ʹ�ü����������Դ��������ǰ������Դ����
	���������ǰ��Դ��������0 ��ô�ź������ڴ���״̬
		  �����ǰ��Դ��������0 ��ô�ź������ڷǴ���״̬
		  ϵͳ���Բ�������ǰ��Դ������Ϊ����
		  ��ǰ��Դ�������Բ�����������Դ����
	*/
	class CORE_EXPORT SmtSemaphore 
	{  
	public:  
		SmtSemaphore (long init_count/*��ʼ�ɹ�ʹ����Դ����*/, 
					  long max_count = (std::numeric_limits<long>::max)()/*���ɹ�ʹ����Դ����*/,
					  const char *szName = "")  
		{  
			assert (init_count >= 0 && max_count > 0 && init_count <= max_count); 

			if (strlen(szName) > 0)
				m_hSemaphore = CreateSemaphoreA(NULL, init_count, max_count, szName);  
			else
				m_hSemaphore = CreateSemaphoreA(NULL, init_count, max_count, NULL);   
		}  

		~SmtSemaphore (void)  
		{  
			CloseHandle (m_hSemaphore);  
		}  

	public:  
		int post (long count = 1)  
		{//���ӵ�ǰ��Դ���� ����count �� 
			BOOL bret = ReleaseSemaphore (m_hSemaphore, count, NULL);  
			return bret ? 0 : -1;  
		}  

		int wait (long timeout = -1)  
		{//�ȴ��ź���,���ڲ��ȴ����������ź�����ǰ��Դ������
			//1.�������ֵ����0���ź������ڴ���״̬������ô������Ѽ�������1���õ����̼߳������С�
			//������ϵͳ������Դ�Ƿ���ã�����������Դ�����ݼ������������̲߳��ᱻ����̴߳�ϡ�
			//ֻ�е���Դ�����ݼ����֮��ϵͳ�Ż���������һ���߳��������Դ�ķ���
			//2.�������ֵ����0����ôϵͳ���õ����߳̽���ȴ�״̬��������һ���߳̽��ź����ĵ�ǰ��
			//Դ��������ʱ��ϵͳ�Ὣ�Ǹ����ڵȴ����̱߳�ɿɵ���״̬
			DWORD ret = WaitForSingleObject (m_hSemaphore, timeout);  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

		HANDLE handle (void)  
		{  
			return m_hSemaphore;  
		}  

	protected:  
		HANDLE m_hSemaphore;  

	};  

	class CORE_EXPORT SmtCondition  
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
		SmtMutex &mutex (void)  
		{  
			return m_mutex;  
		}  

		/// Signal one waiting thread.  
		int signal (void)  
		{ //���� 
			// must hold the external mutex before enter  
			if ( m_lWaiters > 0 )  
				m_semaphore.post ();  

			return 0;  
		}  

		/// Signal *all* waiting threads.  
		int broadcast (void)  
		{  //���� 
			// must hold the external mutex before enter  
			if ( m_lWaiters > 0 )  
				m_semaphore.post (m_lWaiters);  

			return 0;  
		}  

		int wait (unsigned long wait_time = -1)  
		{  
			// must hold the external mutex before enter  
			int ret = 0;  
			m_lWaiters++;  

			ret = SignalObjectAndWait (m_mutex.handle (),
				m_semaphore.handle (), wait_time, FALSE); 

			m_mutex.acquire();  

			m_lWaiters --;  

			return ret == WAIT_OBJECT_0 ? 0 : -1;  
		}  

	protected:  
		SmtMutex		m_mutex;  

		long			m_lWaiters;		// Number of waiting threads.  
		SmtSemaphore	m_semaphore;	// Queue up threads waiting for the condition to become signaled. 
	};  
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_TRD_SYNC_H