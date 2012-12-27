/*
File:    smt_msg_def.h

Desc:    SmartGis 消息结构

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MSG_DEF_H
#define _SMT_MSG_DEF_H

#define SMT_FUNC_NAME_LENGTH				50
#define SMT_GROUP_NAME_LENGTH				50

#include "smt_core.h"
#include <map>
//#include <hash_map>
//////////////////////////////////////////////////////////////////////////
namespace Smt_Core
{
	struct SmtFuncItem 
	{
		char									szName[SMT_FUNC_NAME_LENGTH];
		long									lMsg;
	};

	enum SmtFuncItemStyle
	{
		FIM_2DVIEW					= 1<<0,
		FIM_3DVIEW					= 1<<1,
		FIM_3DEXVIEW				= 1<<2,
		FIM_MAPDOCCATALOG			= 1<<3,
		FIM_2DMFTOOLBAR				= 1<<4,
		FIM_3DMFTOOLBAR				= 1<<5,
		FIM_2DMFMENU				= 1<<6,
		FIM_3DMFMENU				= 1<<7,
		FIM_AUXMODULEBOX			= 1<<8,
		FIM_AUXMODULETREE			= 1<<9,
	};

	struct SmtListenerMsg
	{
		HWND	hSrcWnd;
		WPARAM	wParam;
		LPARAM  lParam;
		void	*pToFollow;
		bool	bModify;

		SmtListenerMsg()
		{
			hSrcWnd		= NULL;
			wParam		= NULL;
			lParam		= NULL;
			pToFollow	= NULL;
			bModify		= false;
		}

		operator LPARAM()
		{
			return LPARAM(this);
		}

		operator WPARAM()
		{
			return WPARAM(this);
		}
	};

#define SMT_MSG_KEY(lMsg,hWnd)				(MAKELONG(lMsg,hWnd))
#define SMT_MSG_KEY_HWND(msgKey)			(HIWORD(msgKey))
#define SMT_MSG_KEY_LMSG(msgKey)			(LOWORD(msgKey))

	typedef vector<SmtFuncItem>					vSmtFuncItems;
	typedef vSmtFuncItems						vSmt2DViewFuncItems;
	typedef vSmtFuncItems						vSmt3DViewFuncItems;
	typedef vSmtFuncItems						vSmt3DExViewFuncItems;
	typedef vSmtFuncItems						vSmtMapDocCatalogFuncItems;
	typedef vSmtFuncItems						vSmt2DToolBarFuncItems;
	typedef vSmtFuncItems						vSmt3DToolBarFuncItems;
	typedef vSmtFuncItems						vSmt2DMenuFuncItems;
	typedef vSmtFuncItems						vSmt3DMenuFuncItems;
	typedef vSmtFuncItems						vSmtAuxModuleBoxFuncItems;
	typedef vSmtFuncItems						vSmtAuxModuleTreeFuncItems;

	typedef vector<long>						vSmtMsgs;
	typedef map<long,void*>						mapMsgToPtr;
	typedef pair<long,void*>					pairMsgToPtr;
	//typedef hash_map<long,void*>				hashmapMsgToPtr;

}

#endif //_SMT_MSG_DEF_H