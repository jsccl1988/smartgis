/*
File:    smt_log.h

Desc:    SmtLog,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_LOG_H
#define _SMT_LOG_H

#include <fstream>

#include "smt_core.h"
#include "smt_cslock.h"

namespace Smt_Core
{
	class SmtLogManager;

	class SMT_EXPORT_CLASS SmtLog
	{
	public:
		friend class SmtLogManager;

		SmtLog(const char * name,const char * filepath,ios::ios_base::openmode mode = ios::out);
		virtual ~SmtLog(void);

	public:
		void					LogMessage(const char * message,...);

	private:
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//多线程安全
#endif
		ofstream				m_log;
		string					m_strLogName;
		string					m_strLogFilePath;
	};

	typedef vector<SmtLog*>		vLogPtrs;
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_LOG_H