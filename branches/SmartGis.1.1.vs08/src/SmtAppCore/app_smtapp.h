/*
File:    app_smtapp.h

Desc:    SmtApp

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.11.3

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef APP_SMTAPP_H
#define APP_SMTAPP_H

#include "smt_core.h"
#include "smt_logmanager.h"
#include "smt_env_struct.h"
#include "smt_msg_def.h"

using namespace Smt_Core;

namespace Smt_App
{
	class SMT_EXPORT_CLASS SmtApp
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
		bool						InitSmtMapService(void);
		bool						InitSmtListenerMgr(void);
		bool						InitSmtAuxModules(void);

	private:
		bool						m_bInit;
	};
}

#if !defined(Export_SmtAppCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtAppCoreD.lib")
#       else
#          pragma comment(lib,"SmtAppCore.lib")
#	    endif  
#endif

#endif //APP_SMTAPP_H