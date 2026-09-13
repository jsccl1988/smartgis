/*
File:    am_msg.h

Desc:    AuxModule msg����ͷ�ļ�

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _AM_MSG_H
#define _AM_MSG_H
#if defined(PLUGIN_EXPORTS)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __declspec(dllimport)
#endif


#include "base/core/msg.h"
#include "plugin/legacy/module_manager.h"
using namespace plugin;

//////////////////////////////////////////////////////////////////////////

#define SMT_AM_MSG_BROADCAST			((SmtAuxModule *)0xFFFF) 
#define SMT_AM_MSG_INVALID				((SmtAuxModule *)0x0000) 

#define	SMT_POST_AM_MSG(pAModule,lMsg,param)\
{\
	SmtAModuleManager * pAModuleMgr = SmtAModuleManager::get_singleton_ptr();\
	pAModuleMgr->notify(pAModule,lMsg,param);\
}
long PLUGIN_EXPORT SmtPostAMMsg(SmtAuxModule *pAMoudule,long lMsg,SmtListenerMsg &param);

#if !defined(PLUGIN_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"plugin_d.lib")
#       else
#          pragma comment(lib,"plugin.lib")
#	    endif  
#endif

#endif //_AM_MSG_H



