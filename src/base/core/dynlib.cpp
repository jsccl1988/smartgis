#include "base/core/dynlib.h"
#include "base/core/logmanager.h"

namespace base
{
	SmtDynLib::SmtDynLib(const char * name,const char * path)
	{
		strcpy(m_szName , name);
		strcpy(m_szPath , path);
		m_hDLL    = NULL;
	}

	SmtDynLib::~SmtDynLib()
	{
       unload();
	}

	bool SmtDynLib::load()
	{
		SmtLog* pLog = SmtLogManager::get_singleton_ptr()->get_log(g_strDynLibLog);
		if (pLog) 
			pLog->log_message("   loading %s",m_szName);	

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
			::MessageBox(NULL,strErr.c_str(),"SmtGis - error", MB_OK | MB_ICONERROR);

			return false;
		}

		SMT_SAFE_DELETE_A(pChPath);

		return true;
	}

	void SmtDynLib::unload()
	{
		if (m_hDLL != NULL)
		{
			SmtLog* pLog = SmtLogManager::get_singleton_ptr()->get_log(g_strDynLibLog);
			if (pLog) 
				pLog->log_message("   unloading %s",m_szName);	

			FreeLibrary(m_hDLL);
			m_hDLL = NULL;
		}
	}
}
