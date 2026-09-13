/*
File:    smt_pluginmanager.h

Desc:    SmartGis ���������,�̰߳�ȫ

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_PLUGINMANAGER_H
#define _SMT_PLUGINMANAGER_H

#include "base/core/core.h"
#include "base/core/plugin.h"
#include "base/core/cslock.h"

namespace base
{
    class CORE_EXPORT SmtPluginManager
    {
    public:
		virtual ~SmtPluginManager(void);

	public:
		static SmtPluginManager*	get_singleton_ptr(void);
		static void					destroy_instance(void);

	public:
		void						load_all_plugin(const char * filePath);
		void						unload_all_plugin(void);

	protected:
		void						start_all_plugin(void);
		void						stop_all_plugin(void);
		SmtPlugin*					load_plugin(const char * name,const char * path);
		void						unload_plugin(const char * name);

    protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock					m_cslock;										//���̰߳�ȫ
#endif
		vSmtPluginPtrs				m_vPluginPtrs;

    private:
		SmtPluginManager(void);

	private:
        static SmtPluginManager*	m_pSingleton;
    };
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif