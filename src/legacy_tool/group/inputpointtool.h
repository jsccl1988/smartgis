/*
File:    gt_inputpointtool.h

Desc:    SmtInputPointTool,���ӵ�Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INPUT_POINT_H
#define _GT_INPUT_POINT_H

#include "legacy_tool/group/basetool.h"
#include "tool/gestures.h"

namespace tool
{
	class SmtInputPointTool:public SmtBaseTool
	{
	public:
		SmtInputPointTool();
		virtual ~SmtInputPointTool();

		int                      Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                      AuxDraw();

	public:
		int                      notify(long nMsg,SmtListenerMsg &param);
		void					 apply_draft(const tool::Draft& draft) override;

	protected:
		void					 OnSetPointType(void);
		void					 EndAppendPoint();

	protected:
		OGRGeometry				 *m_pGeom;
		ushort					 m_appendType;
		float					 m_fAngle;
	};
}

#endif //_GT_INPUT_POINT_H
