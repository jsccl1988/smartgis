/*
File:    gt_flashtool.h

Desc:    SmtFlashTool,…¡À∏π§æﬂ

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_FLASH_TOOL_H
#define _GT_FLASH_TOOL_H

#include "gt_basetool.h"

namespace Smt_GroupTool
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
		int					Notify(long nMsg,SmtListenerMsg &param);

	protected:
		SmtVectorLayer		*m_pResultLayer;
		
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
