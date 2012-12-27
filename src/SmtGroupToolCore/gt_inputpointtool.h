/*
File:    gt_inputpointtool.h

Desc:    SmtInputPointTool,添加点要素工具

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INPUT_POINT_H
#define _GT_INPUT_POINT_H

#include "gt_basetool.h"

namespace Smt_GroupTool
{
	class SmtInputPointTool:public SmtBaseTool
	{
	public:
		SmtInputPointTool();
		virtual ~SmtInputPointTool();

		int                      Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                      AuxDraw();

	public:
		int                      Notify(long nMsg,SmtListenerMsg &param);

		int                      LButtonDown(uint nFlags,lPoint point);
		int                      MouseMove(uint nFlags,lPoint point);
		int                      LButtonUp(uint nFlags,lPoint point);
		int                      RButtonDown(uint nFlags,lPoint point);
		int                      MouseWeel(uint nFlags, short zDelta,lPoint point);
		
	private:
		void                     AppendChildImage(short mouse_status,lPoint point);
		void                     AppendText(short mouse_status,lPoint point);
		void                     AppendDot(short mouse_status,lPoint point);

		void					 EndAppendPoint();
		
	protected:
		SmtGeometry				 *m_pGeom;
		ushort					 m_appendType;
		float					 m_fAngle;

	private:
		lPoint					 m_pntOrigin;
		lPoint					 m_pntCur;
		lPoint					 m_pntPrev;
		BOOL                     m_bIsDrag;
		bool					 m_bDelay;
	};
}

#endif //_GT_INPUT_POINT_H
