/*
File:    am_amodule.h

Desc:    SmartGis Aux Module

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _AM_AMODULE_H
#define _AM_AMODULE_H

#include "smt_listener.h"
#include "smt_msg.h"

using namespace Smt_Core;

namespace Smt_AM
{
	class SMT_EXPORT_CLASS SmtAuxModule:public SmtListener
	{
	public:
		SmtAuxModule(void);
		virtual ~SmtAuxModule(void);

	public:
		virtual int							Register(void);
		virtual int							RegisterMsg(void);

		virtual int							UnRegister(void);
		virtual int							UnRegisterMsg(void);

		virtual int							SetActive();

	public:
		virtual int							Init(void);
		virtual int							Destroy(void);
	};

	typedef vector<SmtAuxModule*>			vSmtAModulePtrs;
}

#if !defined(Export_SmtAuxModule)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtAuxModuleD.lib")
#       else
#          pragma comment(lib,"SmtAuxModule.lib")
#	    endif  
#endif

#endif //_AM_AMODULE_H