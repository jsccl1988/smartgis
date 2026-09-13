/*
File:    gt_inputregiontool.h

Desc:    SmtInputRegionTool,������Ҫ�ع���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INPUT_REGION_TOOL_H
#define _GT_INPUT_REGION_TOOL_H

#include "legacy_tool/group/basetool.h"
#include "tool/gestures.h"

namespace tool
{
	class SmtInputRegionTool:public SmtBaseTool
	{
	public:
		SmtInputRegionTool();
		virtual ~SmtInputRegionTool();

		int                Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                AuxDraw();

	public:
		int                notify(long nMsg,SmtListenerMsg &param);
		void               apply_draft(const tool::Draft& draft) override;
		int                KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	protected:
		void               OnSetRegionType(void);
		void               EndAppendRegion();

	protected:
		OGRGeometry			*m_pGeom;
		ushort              m_appendType;
	};
}

#endif //_GT_INPUT_REGION_TOOL_H
