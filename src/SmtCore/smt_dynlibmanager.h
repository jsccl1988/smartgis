/*
File:    smt_dynlibmanager.h

Desc:    SmartGis 动态库管理类,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_DYNLIBMANAGER_H
#define _SMT_DYNLIBMANAGER_H

#include "smt_core.h"
#include "smt_dynlib.h"
#include "smt_cslock.h"

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtDynLibManager
	{
	public:
		virtual ~SmtDynLibManager(void);

	public:
		static SmtDynLibManager*	GetSingletonPtr(void);
		static void					DestoryInstance(void);

	public:
		SmtDynLib *					LoadDynLib(const char * name,const char * path);
		void						UnLoadDynLib(const char * name);

	protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock					m_cslock;										//多线程安全
#endif
		vSmtDynLibPtrs				m_vDynLibPtrs;

	private:
		SmtDynLibManager(void);

	private:
        static SmtDynLibManager * m_pSingleton;

	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_DYNLIBMANAGER_H