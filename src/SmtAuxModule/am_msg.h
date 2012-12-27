/*
File:    am_msg.h

Desc:    AuxModule msg基础头文件

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _AM_MSG_H
#define _AM_MSG_H

#include "smt_msg.h"
#include "am_amodulemanager.h"
using namespace Smt_AM;

//////////////////////////////////////////////////////////////////////////

#define SMT_AM_MSG_BROADCAST			((SmtAuxModule *)0xFFFF) 
#define SMT_AM_MSG_INVALID				((SmtAuxModule *)0x0000) 

#define	SMT_POST_AM_MSG(pAModule,lMsg,param)\
{\
	SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();\
	pAModuleMgr->Notify(pAModule,lMsg,param);\
}
long SMT_EXPORT_API SmtPostAMMsg(SmtAuxModule *pAMoudule,long lMsg,SmtListenerMsg &param);

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_AM_MSG_H



