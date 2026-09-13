#include <time.h>

#include "base/core/log.h"
#include "base/core/core_exception.h"
#include "base/core/api.h"

namespace base
{
	SmtLog::SmtLog(const char * name,const char * filepath,ios::ios_base::openmode mode)
	{
		string strFileName = name;
		string strEleName  = name;

		if (strlen(filepath) == 0)
		{
			m_strLogFilePath = get_app_path() + "log\\";
		}
		else
			m_strLogFilePath = filepath;

		m_strLogName = name;

		string::size_type n = strEleName.rfind(".");
		if ( n != string::npos)
		{
			string name = strEleName;
			strEleName = name.substr(0, n);
		}
		else
		{
			strFileName += ".log";
		}

		string allpath = m_strLogFilePath +strFileName;

		locale loc1 = locale::global(locale(".936"));
		m_log.open(allpath.c_str(),mode);
		locale::global(locale(loc1));

		if (m_log.fail())
		{
#if     defined( _DEBUG)
			string desc,src;
			desc = "Can not open this file";
			desc += m_strLogName;
			src = "SmtLog::SmtLog";	  
			SMT_EXCEPT(ERR_FILE_NOT_FIND,desc,src);
#endif	
			return;
		}
	}

	SmtLog::~SmtLog()
	{
		m_log.close();
	}

	void SmtLog::log_message(const char * message,...)
	{
		char text[1024];
		memset(text,'\0',1024);
		va_list args;

		time_t ttime;
		tm* pCurTime;
		char szLogTime[32];

		time( &ttime );
		pCurTime= localtime( &ttime );
		strftime( szLogTime, 32, "%H:%M:%S  ", pCurTime );

		va_start(args, message);
		vsprintf(text, message, args);
		va_end(args);

#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		m_log << szLogTime <<text <<  endl;
		m_log.flush();

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif
	}
}
