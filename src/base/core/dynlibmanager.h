/*
File:    smt_dynlibmanager.h

Desc:    SmartGis ��̬�������,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_DYNLIBMANAGER_H
#define _SMT_DYNLIBMANAGER_H

#include "base/core/core.h"
#include "base/core/dynlib.h"
#include "base/core/cslock.h"

namespace base
{
	class CORE_EXPORT SmtDynLibManager
	{
	public:
		virtual ~SmtDynLibManager(void);

	public:
		static SmtDynLibManager*	get_singleton_ptr(void);
		static void					destroy_instance(void);

	public:
		SmtDynLib *					load_dyn_lib(const char * name,const char * path);
		void						unload_dyn_lib(const char * name);

	protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock					m_cslock;										//���̰߳�ȫ
#endif
		vSmtDynLibPtrs				m_vDynLibPtrs;

	private:
		SmtDynLibManager(void);

	private:
        static SmtDynLibManager * m_pSingleton;

	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif //_SMT_DYNLIBMANAGER_H