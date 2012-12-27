/*
File:    gt_viewtool.h

Desc:    SmtViewCtrlTool,ä¯ÀÀ¹¤¾ß

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_2DVIEW_CONTROL_TOOL_H
#define _GT_2DVIEW_CONTROL_TOOL_H

#include "gt_basetool.h"

namespace Smt_GroupTool
{
	enum CursorType
	{
		CursorLoupePlus,
		CursorLoupeMinus,
		CursorMove,
		CursorIdentify
	};

	class SmtViewCtrlTool:public SmtBaseTool
	{
	public:
		SmtViewCtrlTool();
		virtual ~SmtViewCtrlTool();

		int					Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int					AuxDraw();
		int					Timer();

	public:
		int					Notify(long nMsg,SmtListenerMsg &param);

		int					SetCursor(void);
		int					LButtonDown(uint nFlags,lPoint point);
		int					MouseMove(uint nFlags,lPoint point);
		int					LButtonUp(uint nFlags,lPoint point);
		int					RButtonDown(uint nFlags,lPoint point);
		int					MouseWeel(uint nFlags, short zDelta,lPoint point);
	protected:
		void				ZoomMove(short mouse_status,lPoint point);
		void				ZoomIn(short mouse_status,lPoint point);
		void				ZoomOut(short mouse_status,lPoint point);
		void				ZoomRestore();
		void				ZoomRefresh();

	protected:
		eViewMode           m_viewMode;
		ushort				m_usFlashed;

	protected:
		lPoint				m_pntOrigin;
		lPoint				m_pntCur;
		lPoint				m_pntPrev;
		BOOL                m_bCaptured;
		HCURSOR             m_hCursors[4];
	};
}

#endif //_GT_2DVIEW_CONTROL_TOOL_H
