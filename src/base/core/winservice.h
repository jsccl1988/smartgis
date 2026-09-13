/*
File:    smt_winservice.h

Desc:    SmartGis , windows ���������,��Ϊʹ�þ�̬��Ա��¼��ǰthisָ�룬
		 ��˸��༰��̳�����һ��������ֻ������һ��������ڡ�

Version: Version 1.0

Writter:  �´���

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_WINSERVICE_H
#define _SMT_WINSERVICE_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"

#define	SMT_WINSERVICE_NAME_LENGTH		50
#define	SMT_WINSERVICE_LOGNAME_LENGTH	50

namespace base
{
	class CORE_EXPORT SmtWinService 
	{
	public:
		//�̳��������д���溯��
		static long					new_(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog);			
		static long					delete_(SmtWinService *&pWinService);			

	protected:
		SmtWinService(const char *szWinServiceName,const char *szWinServiceLog);
		virtual ~SmtWinService();

	public:
		virtual long				user_run();						//�û��������ѭ������
	
		virtual long				user_ctrl(uchar dwOpcode);		//�û��������ѭ������

	public:
		BOOL						is_installed();

		BOOL						init();
		BOOL						install();
		BOOL						uninstall();
		BOOL						start();
		BOOL						restart();
		BOOL						stop();

		void						start_ctrl_dispatcher();

	protected:				
		static void  __stdcall		service_main(DWORD dwNumServicesArgs,LPWSTR  *lpServiceArgVectors);
		static void  __stdcall		service_ctrl(DWORD dwOpcode);

	protected:
		void						log_event(LPCTSTR pszFormat, ...);

	public:
		char						m_szServiceName[SMT_WINSERVICE_NAME_LENGTH];
		char						m_szServerLog[SMT_WINSERVICE_LOGNAME_LENGTH];

		BOOL						m_bInstall;
		SERVICE_STATUS_HANDLE		m_hServiceStatus;
		SERVICE_STATUS				m_ServiceStatus;

	protected:
		static SmtWinService*		m_pWinService;				
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_WINSERVICE_H