/*
File:    smt_pluginmanager.h

Desc:    SmartGis 插件管理类,线程安全

Version: Version 1.0

Writter:  陈春亮

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SMT_PLUGINMANAGER_H
#define _SMT_PLUGINMANAGER_H

#include "smt_core.h"
#include "smt_plugin.h"
#include "smt_cslock.h"

namespace Smt_Core
{
    class SMT_EXPORT_CLASS SmtPluginManager
    {
    public:
		virtual ~SmtPluginManager(void);

	public:
		static SmtPluginManager*	GetSingletonPtr(void);
		static void					DestoryInstance(void);

	public:
		void						LoadAllPlugin(const char * filePath);
		void						UnLoadAllPlugin(void);

	protected:
		void						StartAllPlugin(void);
		void						StopAllPlugin(void);
		SmtPlugin*					LoadPlugin(const char * name,const char * path);
		void						UnLoadPlugin(const char * name);

    protected:
#ifdef SMT_THREAD_SAFE
		SmtCSLock					m_cslock;										//多线程安全
#endif
		vSmtPluginPtrs				m_vPluginPtrs;

    private:
		SmtPluginManager(void);

	private:
        static SmtPluginManager*	m_pSingleton;
    };
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif