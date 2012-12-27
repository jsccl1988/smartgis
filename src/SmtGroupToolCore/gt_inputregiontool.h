/*
File:    gt_inputregiontool.h

Desc:    SmtInputRegionTool,添加区要素工具

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_INPUT_REGION_TOOL_H
#define _GT_INPUT_REGION_TOOL_H

#include "gt_basetool.h"

namespace Smt_GroupTool
{
	class SmtInputRegionTool:public SmtBaseTool
	{
	public:
		SmtInputRegionTool();
		virtual ~SmtInputRegionTool();

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
		void               AppendFan(uint mouse_status,lPoint point);
		void               AppendRect(uint mouse_status,lPoint point);
		void               AppendPolygon(uint mouse_status,lPoint point);
	
		void               EndAppendRegion();

	protected:
		SmtGeometry			*m_pGeom;
		ushort              m_appendType;

	private:
		double              m_fScaleDelt;
		HCURSOR             m_hCrossCursor;

		lPoint				m_pntOrigin;
		lPoint				m_pntCur;
		lPoint				m_pntPrev;
		bool                m_bIsDrag;
		bool				m_bDelay;
		int                 m_nStep;
	};
}

#endif //_GT_INPUT_REGION_TOOL_H
