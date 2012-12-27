#include "smt_winservice.h"

#include "smt_logmanager.h"
#include "smt_api.h"

#include <stdio.h>
#include <strsafe.h>

namespace Smt_Core
{
	MEMORYSTATUS g_memory;
	SmtWinService* SmtWinService::m_pWinService = NULL;

	SmtWinService::SmtWinService(const char *szWinServiceName,const char *szWinServiceLog)
	{
		m_szServiceName[0] = '\0';
		m_szServerLog[0]= '\0';

		sprintf_s(m_szServiceName,SMT_WINSERVICE_NAME_LENGTH,szWinServiceName);

		if (strlen(szWinServiceLog) == 0)
			sprintf_s(m_szServerLog,SMT_WINSERVICE_LOGNAME_LENGTH,szWinServiceName);
		else
			sprintf_s(m_szServerLog,SMT_WINSERVICE_LOGNAME_LENGTH,szWinServiceLog);
	}

	SmtWinService::~SmtWinService()
	{

	}

	BOOL SmtWinService::IsInstalled()
	{
		BOOL bResult = FALSE;

		//打开服务控制管理器
		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM != NULL)
		{
			//打开服务
			SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_QUERY_CONFIG);
			if (hService != NULL)
			{
				bResult = TRUE;
				::CloseServiceHandle(hService);
			}
			::CloseServiceHandle(hSCM);
		}
		return bResult;
	}

	BOOL SmtWinService::Init()
	{
		m_hServiceStatus = NULL;
		m_ServiceStatus.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
		m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_PAUSE_CONTINUE;
		m_ServiceStatus.dwWin32ExitCode = 0;
		m_ServiceStatus.dwServiceSpecificExitCode = 0;
		m_ServiceStatus.dwCheckPoint = 0;
		m_ServiceStatus.dwWaitHint = 0;

		sprintf_s(m_szServerLog,MAX_PATH,"%s",m_szServiceName);

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pServerLog = NULL;
		if (NULL == (pServerLog = pLogMgr->GetLog(m_szServerLog)))
		{
			pServerLog = pLogMgr->CreateLog(m_szServerLog,ios::ios_base::out|ios::ios_base::app);
		}

		return TRUE;
	}

	BOOL SmtWinService::Install()
	{
		if (IsInstalled())
			return TRUE;

		//打开服务控制管理器
		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (hSCM == NULL)
		{
			MessageBox(NULL,("Couldn't open service manager"), m_szServiceName, MB_OK);
			return FALSE;
		}

		// Get the executable file path
		TCHAR szFilePath[MAX_PATH];
		::GetModuleFileName(NULL, szFilePath, MAX_PATH);

		//创建服务
		SC_HANDLE hService = ::CreateService(
			hSCM, m_szServiceName, m_szServiceName,
			SERVICE_ALL_ACCESS, SERVICE_WIN32_SHARE_PROCESS,
			SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
			szFilePath, NULL, NULL,  (""), NULL, NULL);

		if (hService == NULL)
		{
			::CloseServiceHandle(hSCM);
			MessageBox(NULL,("Couldn't create service"), m_szServiceName, MB_OK);
			return FALSE;
		}

		::CloseServiceHandle(hService);
		::CloseServiceHandle(hSCM);

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pServerLog = NULL;
		if (NULL != (pServerLog = pLogMgr->GetLog(m_szServerLog)))
		{
			pServerLog->LogMessage("Install OK");
		}

		return TRUE;
	}

	BOOL SmtWinService::Uninstall()
	{
		if (!IsInstalled())
			return TRUE;

		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM == NULL)
		{
			MessageBox(NULL,("Couldn't open service manager"), m_szServiceName, MB_OK);
			return FALSE;
		}

		SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_STOP | DELETE);

		if (hService == NULL)
		{
			::CloseServiceHandle(hSCM);
			MessageBox(NULL,  ("Couldn't open service"), m_szServiceName, MB_OK);
			return FALSE;
		}
		SERVICE_STATUS m_ServiceStatus;
		::ControlService(hService, SERVICE_CONTROL_STOP, &m_ServiceStatus);

		//删除服务
		BOOL bDelete = ::DeleteService(hService);
		::CloseServiceHandle(hService);
		::CloseServiceHandle(hSCM);

		if (bDelete)
		{
			SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
			SmtLog *pServerLog = NULL;
			if (NULL != (pServerLog = pLogMgr->GetLog(m_szServerLog)))
			{
				pServerLog->LogMessage("Uninstall OK");
			}

			return TRUE;
		}

		MessageBox(NULL,("Service could not be deleted"), m_szServiceName, MB_OK);
		LogEvent( ("Service could not be deleted"));

		return FALSE;
	}

	BOOL SmtWinService::Start()
	{
		if (!IsInstalled())
			return FALSE;

		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM == NULL)
		{
			MessageBox(NULL,  ("Couldn't open service manager"), m_szServiceName, MB_OK);

			return FALSE;
		}

		SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_ALL_ACCESS);

		if (hService == NULL)
		{
			::CloseServiceHandle(hSCM);
			MessageBox(NULL,  ("Couldn't open service"), m_szServiceName, MB_OK);

			return FALSE;
		}

		// 启动服务
		if( ::StartService( hService, NULL, NULL) == FALSE)
		{
			MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}
		// 等待服务启动
		while( ::QueryServiceStatus( hService, &m_ServiceStatus) == TRUE)
		{
			::Sleep( m_ServiceStatus.dwWaitHint);
			if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
			{
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
				SmtLog *pServerLog = NULL;
				if (NULL != (pServerLog = pLogMgr->GetLog(m_szServerLog)))
				{
					pServerLog->LogMessage("Start OK");
				}

				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL SmtWinService::Restart()
	{
		if (!IsInstalled())
			return FALSE;

		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM == NULL)
		{
			MessageBox(NULL,  ("Couldn't open service manager"), m_szServiceName, MB_OK);

			return FALSE;
		}

		SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_ALL_ACCESS);

		if (hService == NULL)
		{
			::CloseServiceHandle(hSCM);
			MessageBox(NULL,  ("Couldn't open service"), m_szServiceName, MB_OK);

			return FALSE;
		}
		// 获得服务的状态
		SERVICE_STATUS m_ServiceStatus;
		if( ::QueryServiceStatus( hService, &m_ServiceStatus) == FALSE)
		{
			MessageBox(NULL,  ("Get Service state error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}
		if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
		{
			// 停止服务
			if( ::ControlService( hService, SERVICE_CONTROL_STOP, &m_ServiceStatus) == FALSE)
			{
				MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				return FALSE;
			}
			// 等待服务停止
			while( ::QueryServiceStatus( hService, &m_ServiceStatus) == TRUE)
			{
				::Sleep( m_ServiceStatus.dwWaitHint);
				if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
				{
					::CloseServiceHandle( hService);
					::CloseServiceHandle( hSCM);
				}
			}
		}

		// 启动服务
		if( ::StartService( hService, NULL, NULL) == FALSE)
		{
			MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}
		// 等待服务启动
		while( ::QueryServiceStatus( hService, &m_ServiceStatus) == TRUE)
		{
			::Sleep( m_ServiceStatus.dwWaitHint);
			if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
			{
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
				SmtLog *pServerLog = NULL;
				if (NULL != (pServerLog = pLogMgr->GetLog(m_szServerLog)))
				{
					pServerLog->LogMessage("Restart OK");
				}

				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL SmtWinService::Stop()
	{
		if (!IsInstalled())
			return FALSE;

		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM == NULL)
		{
			MessageBox(NULL,  ("Couldn't open service manager"), m_szServiceName, MB_OK);

			return FALSE;
		}

		SC_HANDLE hService = ::OpenService(hSCM, m_szServiceName, SERVICE_STOP);

		if (hService == NULL)
		{
			::CloseServiceHandle(hSCM);
			MessageBox(NULL,  ("Couldn't open service"), m_szServiceName, MB_OK);

			return FALSE;
		}

		// 停止服务
		if( ::ControlService( hService, SERVICE_CONTROL_STOP, &m_ServiceStatus) == FALSE)
		{
			MessageBox(NULL,  ("stop service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}

		// 等待服务停止
		while(m_ServiceStatus.dwCurrentState==SERVICE_STOP_PENDING)         
		{
			Sleep(10);
			QueryServiceStatus(hService,&m_ServiceStatus);
		}
		if(m_ServiceStatus.dwCurrentState==SERVICE_STOPPED)
		{
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
			SmtLog *pServerLog = NULL;
			if (NULL != (pServerLog = pLogMgr->GetLog(m_szServerLog)))
			{
				pServerLog->LogMessage("Stop OK");
			}

			return TRUE;
		}

		::CloseServiceHandle( hService);
		::CloseServiceHandle( hSCM);

		return FALSE;
	}

	void SmtWinService::StartCtrlDispatcher()
	{
		SERVICE_TABLE_ENTRY st[] =
		{
			{ m_szServiceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
			{ NULL, NULL }
		};

		if (!::StartServiceCtrlDispatcher(st))
		{
			LogEvent("Register Service Main Function Error!");
		}
	}

	void SmtWinService::LogEvent(LPCTSTR pFormat, ...)
	{
		TCHAR    chMsg[256];
		HANDLE  hEventSource;
		LPTSTR  lpszStrings[1];
		va_list pArg;

		va_start(pArg, pFormat);
		_vstprintf(chMsg, pFormat, pArg);
		va_end(pArg);

		lpszStrings[0] = chMsg;

		hEventSource = RegisterEventSource(NULL, m_szServiceName);
		if (hEventSource != NULL)
		{
			ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (LPCTSTR*) &lpszStrings[0], NULL);
			DeregisterEventSource(hEventSource);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtWinService::UserRun()
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pServerLog = NULL;
		if (NULL == (pServerLog = pLogMgr->GetLog(m_szServerLog)))
			return SMT_ERR_FAILURE;

		static char buffer[16];

		while (m_pWinService->m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
		{
			GlobalMemoryStatus(&g_memory);
			sprintf(buffer, "%d", g_memory.dwAvailPhys);
			pServerLog->LogMessage(buffer);

			Sleep(50000);
		}
		
		return SMT_ERR_NONE;
	}

	long SmtWinService::UserCtrl(uchar dwOpcode)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pServerLog = NULL;
		if (NULL == (pServerLog = pLogMgr->GetLog(m_szServerLog)))
			return SMT_ERR_FAILURE;

		pServerLog->LogMessage("user ctrl");

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void __stdcall SmtWinService::ServiceMain(DWORD   dwNumServicesArgs,LPWSTR  *lpServiceArgVectors)
	{
		// Register the control request handler
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();

		if (NULL == m_pWinService)
		{
			SmtLog *pLog = NULL;
			if (NULL == (pLog = pLogMgr->GetDefaultLog()))
				return;

			pLog->LogMessage("static this is null");
	
			return;
		}
		
		SmtLog *pServerLog = NULL;
		if (NULL == (pServerLog = pLogMgr->GetLog(m_pWinService->m_szServerLog)))
			return;

		pServerLog->LogMessage("ServiceMain");
		for (int i = 0; i < dwNumServicesArgs;i++)
		{
			pServerLog->LogMessage("%d:%s",i,(const char *)lpServiceArgVectors[i]);
		}

		m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
		m_pWinService->m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_PAUSE_CONTINUE; 

		//注册服务控制
		m_pWinService->m_hServiceStatus = RegisterServiceCtrlHandler(m_pWinService->m_szServiceName, ServiceCtrl);
		if (m_pWinService->m_hServiceStatus == NULL)
		{
			m_pWinService->LogEvent( ("Handler not installed"));
			pServerLog->LogMessage("Handler not installed");

			return;
		}

		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		m_pWinService->m_ServiceStatus.dwWin32ExitCode = S_OK;
		m_pWinService->m_ServiceStatus.dwCheckPoint = 0;
		m_pWinService->m_ServiceStatus.dwWaitHint = 0;
		m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		//running
		m_pWinService->UserRun();
	}

	void __stdcall SmtWinService::ServiceCtrl(DWORD dwOpcode)
	{
		switch (dwOpcode)
		{
		case SERVICE_CONTROL_STOP:
			m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			break;
		case SERVICE_CONTROL_PAUSE:
			m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_PAUSED;
			break;
		case SERVICE_CONTROL_CONTINUE:
			m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			break;
		case SERVICE_CONTROL_SHUTDOWN: 
			m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_PAUSED;
			break;
		default:
			m_pWinService->LogEvent("Bad service request");
		}

		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		m_pWinService->UserCtrl(dwOpcode);

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pServerLog = NULL;
		if (NULL == (pServerLog = pLogMgr->GetLog(m_pWinService->m_szServerLog)))
			return;

		pServerLog->LogMessage("ServiceCtrl");	
	}

	long SmtWinService::New(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog)
	{
		if (NULL == m_pWinService)
		{
			SmtWinService::m_pWinService = new SmtWinService(szWinServiceName,szWinServiceLog);
		}

		pWinService = SmtWinService::m_pWinService;
		
		return SMT_ERR_NONE;
	}

	long SmtWinService::Delete(SmtWinService *&pWinService)
	{
		SMT_SAFE_DELETE(SmtWinService::m_pWinService);

		return SMT_ERR_NONE;
	}
}