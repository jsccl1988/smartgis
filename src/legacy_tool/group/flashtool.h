/*
File:    gt_flashtool.h

Desc:    SmtFlashTool,��˸����

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_FLASH_TOOL_H
#define _GT_FLASH_TOOL_H

#include "legacy_tool/group/basetool.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

namespace tool {
class Workspace;
}

namespace tool
{
	class SmtFlashTool:public SmtBaseTool
	{
	public:
		SmtFlashTool();
		virtual ~SmtFlashTool();
		int					Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int					AuxDraw();
		int					Timer();

	public:
		int					notify(long nMsg,SmtListenerMsg &param);

		// Session flag lives on tool::Workspace (ViewHost). Leftover m_bFlash
		// is only used when no workspace is bound.
		void				bind_workspace(tool::Workspace* workspace) {
			m_workspace = workspace;
		}

	protected:
		bool				session_flashing() const;

		sdb::ScratchLayer m_resultLayer;
		tool::Workspace*	m_workspace;
		
		string				m_strFlashStyle1;
		string				m_strFlashStyle2;
		string				m_strFlashStyle;
		eFlashMode			m_flsMode;

		bool				m_bFlash;
		bool				m_bStyle1;
		double				m_fScaleDelt;
	};
}

#endif //_GT_FLASH_TOOL_H
