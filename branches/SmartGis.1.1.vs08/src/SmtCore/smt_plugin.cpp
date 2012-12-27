#include "smt_plugin.h"
#include "smt_logmanager.h"

namespace Smt_Core
{
	SmtPlugin::SmtPlugin(const char * name,const char * path):SmtDynLib(name,path)
	{
		 m_fnGetPluginVersion = NULL;
		 m_fnStartPlugin= NULL;
		 m_fnStopPlugin = NULL;
	}

	SmtPlugin::~SmtPlugin(void)
	{

	}

	bool SmtPlugin::Load()
	{
		SmtLog* pLog = SmtLogManager::GetSingletonPtr()->GetLog(g_strPluginLibLog);
		if (pLog) 
			pLog->LogMessage("   loading plugin :%s",m_szName);	

		int nPathLen = strlen(m_szPath) + MAX_PATH;
		char* pChPath = new char[nPathLen];
		sprintf_s(pChPath, nPathLen, "%s%s", m_szPath, m_szName); 
		m_hDLL = LoadLibrary(pChPath);
		if(!m_hDLL) 
		{
			string strErr = "   Loading ";
			strErr += m_szName;
			strErr += " error!";
			if (pLog) pLog->LogMessage(strErr.c_str());			 
			::MessageBox(NULL,strErr.c_str(),"SmtGis- error", MB_OK | MB_ICONERROR);

			return false;
		}

		SMT_SAFE_DELETE_A(pChPath);
		
		m_fnGetPluginVersion  = reinterpret_cast<fnGetPluginVersion *>(::GetProcAddress(m_hDLL, "GetPluginVersion"));
		m_fnStartPlugin = reinterpret_cast<fnStartPlugin *>(::GetProcAddress(m_hDLL, "StartPlugin"));
		m_fnStopPlugin  = reinterpret_cast<fnStopPlugin *>(::GetProcAddress(m_hDLL, "StopPlugin"));

		// If the functions aren't found, we're going to assume this is
		// a plain simple DLL and not one of our plugins
		if(!m_fnGetPluginVersion || !m_fnStartPlugin || !m_fnStopPlugin)
		{
			::FreeLibrary(m_hDLL);
			string strErr = "   ";
			strErr += m_szName;
			strErr += " is an invalid plugin,so not load it!";
			if (pLog) 
				pLog->LogMessage(strErr.c_str());			 
			::MessageBox(NULL,strErr.c_str(),"SmtGis - error", MB_OK | MB_ICONERROR);
			return false;
		}
			
		return true;
	}

	void SmtPlugin::UnLoad()
	{
		if (m_hDLL != NULL)
		{
			SmtLog* pLog = SmtLogManager::GetSingletonPtr()->GetLog(g_strPluginLibLog);
			if (pLog) 
				pLog->LogMessage("   unloading plugin :%s",m_szName);	

			FreeLibrary(m_hDLL);
			m_hDLL = NULL;
		}
	}
}