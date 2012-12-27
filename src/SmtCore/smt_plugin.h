/*
File:    smt_plugin.h

Desc:    SmartGis ²å¼þÀà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_PLUGIN_H
#define _SMT_PLUGIN_H

#include "smt_core.h"
#include "smt_dynlib.h"

namespace Smt_Core
{
    const string g_strPluginLibLog = "SmtPluginLib";

	class SMT_EXPORT_CLASS SmtPlugin:public SmtDynLib
	{
	public:
        SmtPlugin(const char * name,const char * path);
		virtual ~SmtPlugin(void);

	public:
        inline int				GetPluginVersion(void){return m_fnGetPluginVersion();}

		inline void				StartPlugin(void){return m_fnStartPlugin();}
		inline void				StopPlugin(void){return m_fnStopPlugin();}

		bool					Load(void);
		void					UnLoad(void);
		
	protected:
        typedef int				fnGetPluginVersion();
		typedef void			fnStartPlugin();
		typedef void			fnStopPlugin();

	protected:
	   fnGetPluginVersion		*m_fnGetPluginVersion;
	   fnStartPlugin			*m_fnStartPlugin;
	   fnStopPlugin				*m_fnStopPlugin;
	};

	typedef vector<SmtPlugin*>  vSmtPluginPtrs;
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif