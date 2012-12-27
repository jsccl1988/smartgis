#include <time.h>

#include "smt_log.h"
#include "smt_exception.h"
#include "smt_api.h"

namespace Smt_Core
{
	SmtLog::SmtLog(const char * name,const char * filepath,ios::ios_base::openmode mode)
	{
		string strFileName = name;
		string strEleName  = name;

		if (strlen(filepath) == 0)
		{
			m_strLogFilePath = GetAppPath() + "log\\";
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

		//���ô���ҳΪ�������ģ�936�Ǽ������ĵĴ���ҳ��
		locale loc1 = locale::global(locale(".936"));
		// ������ʹ��std::ifstream ���� std::fstream
		m_log.open(allpath.c_str(),mode);
		//�ָ�ԭ���Ĵ���ҳ
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

	void SmtLog::LogMessage(const char * message,...)
	{
		char text[1024];
		memset(text,'\0',1024);
		va_list args;

		time_t ttime;
		tm* pCurTime;
		char szLogTime[32];

		//get time
		time( &ttime );
		pCurTime= localtime( &ttime );
		strftime( szLogTime, 32, "%H:%M:%S  ", pCurTime );


		//parse the given string
		va_start(args, message);
		vsprintf(text, message, args);
		va_end(args);

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		m_log << szLogTime <<text <<  endl;
		m_log.flush();

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}
}