/*
File:    app_smtapp.h

Desc:    SmtApp

Version: Version 1.0

Writter:  �´���

Date:    2012.11.3

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef APP_SMTAPP_H
#define APP_SMTAPP_H
#if defined(APP_CORE_EXPORTS)
#define APP_CORE_EXPORT __declspec(dllexport)
#else
#define APP_CORE_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "base/core/logmanager.h"
#include "base/core/env_struct.h"
#include "base/core/msg_def.h"

using namespace base;

namespace app
{
	class APP_CORE_EXPORT SmtApp
	{
	public:
		SmtApp(void);
		virtual ~SmtApp(void);

	public:
		bool						Init();
		bool						DelayInit();
		bool						Destory();

	protected:
		bool						InitLogMgr(void);
		bool						InitStyleMgr(void);
		bool						InitSmtDataSource(void);
		bool						InitSmtMap(void);
		bool						InitSmtListenerMgr(void);
		bool						InitSmtAuxModules(void);

	private:
		bool						m_bInit;
	};
}

#if !defined(APP_CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"app_core_d.lib")
#       else
#          pragma comment(lib,"app_core.lib")
#	    endif  
#endif

#endif //APP_SMTAPP_H