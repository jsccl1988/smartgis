/*
File:    am_amodule.h

Desc:    SmartGis Aux Module

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _AM_AMODULE_H
#define _AM_AMODULE_H
#if defined(PLUGIN_EXPORTS)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __declspec(dllimport)
#endif


#include "base/core/listener.h"
#include "base/core/msg.h"

using namespace base;

namespace plugin
{
	class PLUGIN_EXPORT SmtAuxModule:public SmtListener
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

#if !defined(PLUGIN_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"pluginD.lib")
#       else
#          pragma comment(lib,"plugin.lib")
#	    endif  
#endif

#endif //_AM_AMODULE_H