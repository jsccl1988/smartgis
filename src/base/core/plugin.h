/*
File:    smt_plugin.h

Desc:    SmartGis �����

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_PLUGIN_H
#define _SMT_PLUGIN_H

#include "base/core/core.h"
#include "base/core/dynlib.h"

namespace base
{
    const string g_strPluginLibLog = "SmtPluginLib";

	class CORE_EXPORT SmtPlugin:public SmtDynLib
	{
	public:
        SmtPlugin(const char * name,const char * path);
		virtual ~SmtPlugin(void);

	public:
        inline int				get_plugin_version(void){return m_fn_get_plugin_version();}

		inline void				start_plugin(void){return m_fn_start_plugin();}
		inline void				stop_plugin(void){return m_fn_stop_plugin();}

		bool					load(void);
		void					unload(void);
		
	protected:
        typedef int				fn_get_plugin_version();
		typedef void			fn_start_plugin();
		typedef void			fn_stop_plugin();

	protected:
	   fn_get_plugin_version	*m_fn_get_plugin_version;
	   fn_start_plugin			*m_fn_start_plugin;
	   fn_stop_plugin			*m_fn_stop_plugin;
	};

	typedef vector<SmtPlugin*>  vSmtPluginPtrs;
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"coreD.lib")
#       else
#          pragma comment(lib,"core.lib")
#	    endif  
#endif

#endif