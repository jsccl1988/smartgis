#include "sys/winservice.h"

#include "base/core/log.h"
#include "base/core/api.h"

#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

namespace base
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

	BOOL SmtWinService::is_installed()
	{
		BOOL bResult = FALSE;

		// (restored)
		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

		if (hSCM != NULL)
		{
			//�򿪷���
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

	BOOL SmtWinService::init()
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
		LOGGING(LOG_INFO, "winservice init: %s", m_szServerLog);

		return TRUE;
	}

	BOOL SmtWinService::install()
	{
		if (is_installed())
			return TRUE;

		// (restored)
		SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (hSCM == NULL)
		{
			MessageBox(NULL,("Couldn't open service manager"), m_szServiceName, MB_OK);
			return FALSE;
		}

		// Get the executable file path
		TCHAR szFilePath[MAX_PATH];
		::GetModuleFileName(NULL, szFilePath, MAX_PATH);

		//��������
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

		LOGGING(LOG_INFO, "install OK");

		return TRUE;
	}

	BOOL SmtWinService::uninstall()
	{
		if (!is_installed())
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

		//ɾ������
		BOOL bDelete = ::DeleteService(hService);
		::CloseServiceHandle(hService);
		::CloseServiceHandle(hSCM);

		if (bDelete)
		{
			LOGGING(LOG_INFO, "uninstall OK");
			return TRUE;
		}

		MessageBox(NULL,("Service could not be deleted"), m_szServiceName, MB_OK);
		log_event( ("Service could not be deleted"));

		return FALSE;
	}

	BOOL SmtWinService::start()
	{
		if (!is_installed())
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

		// ��������
		if( ::StartService( hService, NULL, NULL) == FALSE)
		{
			MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}
		// �ȴ���������
		while( ::QueryServiceStatus( hService, &m_ServiceStatus) == TRUE)
		{
			::Sleep( m_ServiceStatus.dwWaitHint);
			if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
			{
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				LOGGING(LOG_INFO, "start OK");
				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL SmtWinService::restart()
	{
		if (!is_installed())
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
		// ��÷����״̬
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
			// ֹͣ����
			if( ::ControlService( hService, SERVICE_CONTROL_STOP, &m_ServiceStatus) == FALSE)
			{
				MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				return FALSE;
			}
			// �ȴ�����ֹͣ
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

		// ��������
		if( ::StartService( hService, NULL, NULL) == FALSE)
		{
			MessageBox(NULL,  ("start service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}
		// �ȴ���������
		while( ::QueryServiceStatus( hService, &m_ServiceStatus) == TRUE)
		{
			::Sleep( m_ServiceStatus.dwWaitHint);
			if( m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
			{
				::CloseServiceHandle( hService);
				::CloseServiceHandle( hSCM);

				LOGGING(LOG_INFO, "restart OK");
				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL SmtWinService::stop()
	{
		if (!is_installed())
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

		// ֹͣ����
		if( ::ControlService( hService, SERVICE_CONTROL_STOP, &m_ServiceStatus) == FALSE)
		{
			MessageBox(NULL,  ("stop service error"), m_szServiceName, MB_OK);
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			return FALSE;
		}

		// �ȴ�����ֹͣ
		while(m_ServiceStatus.dwCurrentState==SERVICE_STOP_PENDING)         
		{
			Sleep(10);
			QueryServiceStatus(hService,&m_ServiceStatus);
		}
		if(m_ServiceStatus.dwCurrentState==SERVICE_STOPPED)
		{
			::CloseServiceHandle( hService);
			::CloseServiceHandle( hSCM);

			LOGGING(LOG_INFO, "stop OK");
			return TRUE;
		}

		::CloseServiceHandle( hService);
		::CloseServiceHandle( hSCM);

		return FALSE;
	}

	void SmtWinService::start_ctrl_dispatcher()
	{
		SERVICE_TABLE_ENTRY st[] =
		{
			{ m_szServiceName, (LPSERVICE_MAIN_FUNCTION)service_main },
			{ NULL, NULL }
		};

		if (!::StartServiceCtrlDispatcher(st))
		{
			log_event("register_ Service Main Function Error!");
		}
	}

	void SmtWinService::log_event(LPCTSTR pFormat, ...)
	{
		TCHAR    chMsg[256];
		HANDLE  hEventSource;
		LPTSTR  lpszStrings[1];
		va_list pArg;

		va_start(pArg, pFormat);
		_vstprintf_s(chMsg, _countof(chMsg), pFormat, pArg);
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
	long SmtWinService::user_run()
	{
		static char buffer[16];

		while (m_pWinService->m_ServiceStatus.dwCurrentState == SERVICE_RUNNING)
		{
			GlobalMemoryStatus(&g_memory);
			sprintf(buffer, "%d", g_memory.dwAvailPhys);
			LOGGING(LOG_INFO, "%s", buffer);

			Sleep(50000);
		}
		
		return SMT_ERR_NONE;
	}

	long SmtWinService::user_ctrl(uchar dwOpcode)
	{
		(void)dwOpcode;
		LOGGING(LOG_INFO, "user ctrl");
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void __stdcall SmtWinService::service_main(DWORD   dwNumServicesArgs,LPWSTR  *lpServiceArgVectors)
	{
		if (NULL == m_pWinService)
		{
			LOGGING(LOG_ERROR, "static this is null");
			return;
		}

		LOGGING(LOG_INFO, "service_main");
		for (int i = 0; i < static_cast<int>(dwNumServicesArgs); i++)
		{
			LOGGING(LOG_INFO, "%d:%s", i, (const char*)lpServiceArgVectors[i]);
		}

		m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
		m_pWinService->m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_PAUSE_CONTINUE; 

		m_pWinService->m_hServiceStatus = RegisterServiceCtrlHandler(m_pWinService->m_szServiceName, service_ctrl);
		if (m_pWinService->m_hServiceStatus == NULL)
		{
			m_pWinService->log_event( ("Handler not installed"));
			LOGGING(LOG_ERROR, "Handler not installed");
			return;
		}

		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		m_pWinService->m_ServiceStatus.dwWin32ExitCode = S_OK;
		m_pWinService->m_ServiceStatus.dwCheckPoint = 0;
		m_pWinService->m_ServiceStatus.dwWaitHint = 0;
		m_pWinService->m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		//running
		m_pWinService->user_run();
	}

	void __stdcall SmtWinService::service_ctrl(DWORD dwOpcode)
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
			m_pWinService->log_event("Bad service request");
		}

		SetServiceStatus(m_pWinService->m_hServiceStatus, &(m_pWinService->m_ServiceStatus));

		m_pWinService->user_ctrl(dwOpcode);
		LOGGING(LOG_INFO, "service_ctrl");
	}

	long SmtWinService::new_(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog)
	{
		if (NULL == m_pWinService)
		{
			SmtWinService::m_pWinService = new SmtWinService(szWinServiceName,szWinServiceLog);
		}

		pWinService = SmtWinService::m_pWinService;
		
		return SMT_ERR_NONE;
	}

	long SmtWinService::delete_(SmtWinService *&pWinService)
	{
		SMT_SAFE_DELETE(SmtWinService::m_pWinService);

		return SMT_ERR_NONE;
	}
}
