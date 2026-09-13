/*
File:    smt_log.h

Desc:    SmtLog,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_LOG_H
#define _SMT_LOG_H

#include <fstream>

#include "base/core/core.h"
#include "base/core/cslock.h"

namespace base
{
	class SmtLogManager;

	class CORE_EXPORT SmtLog
	{
	public:
		friend class SmtLogManager;

		SmtLog(const char * name,const char * filepath,ios::ios_base::openmode mode = ios::out);
		virtual ~SmtLog(void);

	public:
		void					log_message(const char * message,...);

	private:
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//���̰߳�ȫ
#endif
		ofstream				m_log;
		string					m_strLogName;
		string					m_strLogFilePath;
	};

	typedef vector<SmtLog*>		vLogPtrs;
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_LOG_H