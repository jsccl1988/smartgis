/*
File:    smt_logmanager.h

Desc:    SmtLogManager,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_LOGMANAGER_H
#define _SMT_LOGMANAGER_H

#include "smt_core.h"
#include "smt_log.h"
#include "smt_cslock.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtLogManager
	{
	private:
		SmtLogManager(void);

	public:
		virtual ~SmtLogManager(void);

		bool					SetDefaultLog(const char * file);
		SmtLog*					GetDefaultLog(void);

	public:
		inline void				SetLogDir(const char *szPath) {m_strLogDir = szPath;}
		SmtLog *				CreateLog(const char * name,ios::ios_base::openmode mode = ios::out);
		void					DestroyLog(const char * name);
		void					DestroyLog(SmtLog *pLog);
		void					DestroyAllLog();

		SmtLog*					GetLog(const string& name);
		const SmtLog*			GetLog(const string& name) const;

	public:
		static SmtLogManager*	GetSingletonPtr(void);
		static void				DestoryInstance(void);

	private:
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//多线程安全
#endif
		string					m_strLogDir;
		vLogPtrs				m_vLogPtrs;
		SmtLog *				m_pDefaultLog;

		static SmtLogManager*	m_pSingleton;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_LOGMANAGER_H