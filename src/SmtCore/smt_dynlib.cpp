#include "smt_dynlib.h"
#include "smt_logmanager.h"

namespace Smt_Core
{
	SmtDynLib::SmtDynLib(const char * name,const char * path)
	{
		strcpy(m_szName , name);
		strcpy(m_szPath , path);
		m_hDLL    = NULL;
	}

	SmtDynLib::~SmtDynLib()
	{
       UnLoad();
	}

	bool SmtDynLib::Load()
	{
		SmtLog* pLog = SmtLogManager::GetSingletonPtr()->GetLog(g_strDynLibLog);
		if (pLog) 
			pLog->LogMessage("   loading %s",m_szName);	

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
			::MessageBox(NULL,strErr.c_str(),"SmtGis - error", MB_OK | MB_ICONERROR);

			return false;
		}

		SMT_SAFE_DELETE_A(pChPath);

		return true;
	}

	void SmtDynLib::UnLoad()
	{
		if (m_hDLL != NULL)
		{
			SmtLog* pLog = SmtLogManager::GetSingletonPtr()->GetLog(g_strDynLibLog);
			if (pLog) 
				pLog->LogMessage("   unloading %s",m_szName);	

			FreeLibrary(m_hDLL);
			m_hDLL = NULL;
		}
	}
}