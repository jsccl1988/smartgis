/*
File:    smt_logmanager.h

Desc:    SmtLogManager,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_LOGMANAGER_H
#define _SMT_LOGMANAGER_H

#include "base/core/core.h"
#include "base/core/log.h"
#include "base/core/cslock.h"

namespace base
{
	class CORE_EXPORT SmtLogManager
	{
	private:
		SmtLogManager(void);

	public:
		virtual ~SmtLogManager(void);

		bool					set_default_log(const char * file);
		SmtLog*					get_default_log(void);

	public:
		inline void				set_log_dir(const char *szPath) {m_strLogDir = szPath;}
		SmtLog *				create_log(const char * name,ios::ios_base::openmode mode = ios::out);
		void					destroy_log(const char * name);
		void					destroy_log(SmtLog *pLog);
		void					destroy_all_log();

		SmtLog*					get_log(const string& name);
		const SmtLog*			get_log(const string& name) const;

	public:
		static SmtLogManager*	get_singleton_ptr(void);
		static void				destroy_instance(void);

	private:
#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;										//���̰߳�ȫ
#endif
		string					m_strLogDir;
		vLogPtrs				m_vLogPtrs;
		SmtLog *				m_pDefaultLog;

		static SmtLogManager*	m_pSingleton;
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_LOGMANAGER_H