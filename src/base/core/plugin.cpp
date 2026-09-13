#include "base/core/plugin.h"
#include "base/core/logmanager.h"

namespace base
{
	SmtPlugin::SmtPlugin(const char * name,const char * path):SmtDynLib(name,path)
	{
		 m_fn_get_plugin_version = NULL;
		 m_fn_start_plugin= NULL;
		 m_fn_stop_plugin = NULL;
	}

	SmtPlugin::~SmtPlugin(void)
	{

	}

	bool SmtPlugin::load()
	{
		SmtLog* pLog = SmtLogManager::get_singleton_ptr()->get_log(g_strPluginLibLog);
		if (pLog) 
			pLog->log_message("   loading plugin :%s",m_szName);	

		int nPathLen = strlen(m_szPath) + MAX_PATH;
		char* pChPath = new char[nPathLen];
		sprintf_s(pChPath, nPathLen, "%s%s", m_szPath, m_szName); 
		m_hDLL = LoadLibrary(pChPath);
		if(!m_hDLL) 
		{
			string strErr = "   Loading ";
			strErr += m_szName;
			strErr += " error!";
			if (pLog) pLog->log_message(strErr.c_str());			 
			::MessageBox(NULL,strErr.c_str(),"SmtGis- error", MB_OK | MB_ICONERROR);

			return false;
		}

		SMT_SAFE_DELETE_A(pChPath);
		
		m_fn_get_plugin_version  = reinterpret_cast<fn_get_plugin_version *>(::GetProcAddress(m_hDLL, "get_plugin_version"));
		m_fn_start_plugin = reinterpret_cast<fn_start_plugin *>(::GetProcAddress(m_hDLL, "start_plugin"));
		m_fn_stop_plugin  = reinterpret_cast<fn_stop_plugin *>(::GetProcAddress(m_hDLL, "stop_plugin"));

		// If the functions aren't found, we're going to assume this is
		// a plain simple DLL and not one of our plugins
		if(!m_fn_get_plugin_version || !m_fn_start_plugin || !m_fn_stop_plugin)
		{
			::FreeLibrary(m_hDLL);
			string strErr = "   ";
			strErr += m_szName;
			strErr += " is an invalid plugin,so not load it!";
			if (pLog) 
				pLog->log_message(strErr.c_str());			 
			::MessageBox(NULL,strErr.c_str(),"SmtGis - error", MB_OK | MB_ICONERROR);
			return false;
		}
			
		return true;
	}

	void SmtPlugin::unload()
	{
		if (m_hDLL != NULL)
		{
			SmtLog* pLog = SmtLogManager::get_singleton_ptr()->get_log(g_strPluginLibLog);
			if (pLog) 
				pLog->log_message("   unloading plugin :%s",m_szName);	

			FreeLibrary(m_hDLL);
			m_hDLL = NULL;
		}
	}
}