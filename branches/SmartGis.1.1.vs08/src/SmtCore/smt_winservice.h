/*
File:    smt_winservice.h

Desc:    SmartGis , windows 服务程序类,因为使用静态成员记录当前this指针，
		 因此该类及其继承类在一个进程中只允许有一个对象存在。

Version: Version 1.0

Writter:  陈春亮

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
		//继承类必须重写下面函数
		static long					New(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog);			
		static long					Delete(SmtWinService *&pWinService);			

	protected:
		SmtWinService(const char *szWinServiceName,const char *szWinServiceLog);
		virtual ~SmtWinService();

	public:
		virtual long				UserRun();						//用户自行完成循环控制
	
		virtual long				UserCtrl(uchar dwOpcode);		//用户自行完成循环控制

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