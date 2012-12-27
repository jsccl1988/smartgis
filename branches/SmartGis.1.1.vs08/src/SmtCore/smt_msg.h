/*
File:    smt_msg.h

Desc:    SmartGis msg基础头文件

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MSG_H
#define _SMT_MSG_H

#include "smt_listenermanager.h"

#define SMT_MSG_INVALID						(-1)		//无效消息	

#define SMT_MSG_SYS_MSG						(0x0000)	//系统消息
#define SMT_MSG_2D_TOOL						(0x1000)	//二维交互工具消息
#define SMT_MSG_3D_TOOL						(0x2000)	//三维交互工具消息
#define SMT_MSG_CMD_BEGIN					(0x3001)	//cmd消息
#define SMT_MSG_CMD_END						(0x4000)	//cmd消息
#define SMT_MSG_USER_BEGIN					(0x4001)	//自定义消息
#define SMT_MSG_USER_END					(0x8000)	//自定义消息

//////////////////////////////////////////////////////////////////////////
#define SMT_LISTENER_MSG_BROADCAST			((Smt_Core::SmtListener *)0xFFFF) 
#define SMT_LISTENER_MSG_INVALID			((Smt_Core::SmtListener *)0x0000) 

#define SMT_MSG_GET_SYS_2DVIEW				(SMT_MSG_SYS_MSG+1)
#define SMT_MSG_GET_SYS_2DEDITVIEW			(SMT_MSG_SYS_MSG+2)
#define SMT_MSG_GET_SYS_3DVIEW				(SMT_MSG_SYS_MSG+3)

#define	SMT_POST_LISTENER_MSG(pListener,lMsg,param)\
{\
	Smt_Core::SmtListenerManager * pListenerMgr = Smt_Core::SmtListenerManager::GetSingletonPtr();\
	pListenerMgr->Notify(pListener,lMsg,param);\
}
long SMT_EXPORT_API SmtPostListenerMsg(Smt_Core::SmtListener *pListener,long lMsg,Smt_Core::SmtListenerMsg &param);

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_MSG_H



