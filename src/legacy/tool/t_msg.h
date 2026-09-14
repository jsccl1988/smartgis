/*
File:    t_msg.h

Desc:    IATool msg����ͷ�ļ�

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _T_MSG_H
#define _T_MSG_H

#include "base/core/msg.h"
#include "legacy/tool/t_iatoolmanager.h"
using namespace tool;

//////////////////////////////////////////////////////////////////////////

#define SMT_IATOOL_MSG_BROADCAST			((SmtIATool *)0xFFFF) 
#define SMT_IATOOL_MSG_INVALID				((SmtIATool *)0x0000) 

#define	SMT_POST_IATOOL_MSG(pIATool,lMsg,param)\
{\
	SmtIAToolManager * pIAToolMgr = SmtIAToolManager::get_singleton_ptr();\
	pIAToolMgr->notify(pIATool,lMsg,param);\
}
TOOL_EXPORT long post_ia_tool_msg(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param);

#endif //_T_MSG_H



