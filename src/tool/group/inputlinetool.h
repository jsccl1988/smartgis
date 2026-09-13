/*
File:    gt_inputlinetool.h

Desc:    SmtInputLineTool,������Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INIPUT_LINE_H
#define _GT_INIPUT_LINE_H

#include "tool/group/basetool.h"
#include "tool/gestures.h"

namespace tool
{
	class SmtInputLineTool:public SmtBaseTool
	{
	public:
		SmtInputLineTool();
		virtual ~SmtInputLineTool();

		int                Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                AuxDraw();

	public:
		int                notify(long nMsg,SmtListenerMsg &param);
		void               apply_draft(const tool::Draft& draft) override;
		int                KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	protected:
		void               OnSetLineType(void);
		void               EndAppendLine();

	protected:
		OGRGeometry			*m_pGeom;
		ushort              m_appendType;
	};
}

#endif //_GT_INIPUT_LINE_H
