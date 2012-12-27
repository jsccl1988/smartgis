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

#include "smt_core.h"
#include "smt_bas_struct.h"

#define	SMT_WINSERVICE_NAME_LENGTH		50
#define	SMT_WINSERVICE_LOGNAME_LENGTH	50

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtWinService 
	{
	public:
		//�̳��������д���溯��
		static long					New(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog);			
		static long					Delete(SmtWinService *&pWinService);			

	protected:
		SmtWinService(const char *szWinServiceName,const char *szWinServiceLog);
		virtual ~SmtWinService();

	public:
		virtual long				UserRun();						//�û��������ѭ������
	
		virtual long				UserCtrl(uchar dwOpcode);		//�û��������ѭ������

	public:
		BOOL						IsInstalled();

		BOOL						Init();
		BOOL						Install();
		BOOL						Uninstall();
		BOOL						Start();
		BOOL						Restart();
		BOOL						Stop();

		void						StartCtrlDispatcher();

	protected:				
		static void  __stdcall		ServiceMain(DWORD dwNumServicesArgs,LPWSTR  *lpServiceArgVectors);
		static void  __stdcall		ServiceCtrl(DWORD dwOpcode);

	protected:
		void						LogEvent(LPCTSTR pszFormat, ...);

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

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_WINSERVICE_H