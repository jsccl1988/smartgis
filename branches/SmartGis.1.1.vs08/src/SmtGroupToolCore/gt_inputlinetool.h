/*
File:    gt_inputlinetool.h

Desc:    SmtInputLineTool,添加线要素工具

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INIPUT_LINE_H
#define _GT_INIPUT_LINE_H

#include "gt_basetool.h"

namespace Smt_GroupTool
{
	class SmtInputLineTool:public SmtBaseTool
	{
	public:
		SmtInputLineTool();
		virtual ~SmtInputLineTool();

		int                Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                AuxDraw();

	public:
		int                Notify(long nMsg,SmtListenerMsg &param);

		int                LButtonDown(uint nFlags,lPoint point);
		int                MouseMove(uint nFlags,lPoint point);
		int                LButtonUp(uint nFlags,lPoint point);
		int                RButtonDown(uint nFlags,lPoint point);
		int                MouseWeel(uint nFlags, short zDelta,lPoint point);
	
	private:
		void               AppendLineString(uint mouse_status,lPoint point);
		void               AppendArc(uint mouse_status,lPoint point);
		void               AppendRect(uint mouse_status,lPoint point);
		void               AppendLinearRing(uint mouse_status,lPoint point);
		void               AppendSpline(uint mouse_status,lPoint point);

		void               EndAppendLine();

	protected:
		SmtGeometry			*m_pGeom;
		ushort              m_appendType;

	private:
		lPoint				m_pntOrigin;
		lPoint				m_pntCur;
		lPoint				m_pntPrev;
		bool                m_bIsDrag;
		int                 m_nStep;
		bool				m_bDelay;
	};
}

#endif //_GT_INIPUT_LINE_H
