/*
File:    t_msg.h

Desc:    IATool msg基础头文件

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _T_MSG_H
#define _T_MSG_H

#include "smt_msg.h"
#include "t_iatoolmanager.h"
using namespace Smt_IATool;

//////////////////////////////////////////////////////////////////////////

#define SMT_IATOOL_MSG_BROADCAST			((SmtIATool *)0xFFFF) 
#define SMT_IATOOL_MSG_INVALID				((SmtIATool *)0x0000) 

#define	SMT_POST_IATOOL_MSG(pIATool,lMsg,param)\
{\
	SmtIAToolManager * pIAToolMgr = SmtIAToolManager::GetSingletonPtr();\
	pIAToolMgr->Notify(pIATool,lMsg,param);\
}
long SMT_EXPORT_API SmtPostIAToolMsg(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param);

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_T_MSG_H



