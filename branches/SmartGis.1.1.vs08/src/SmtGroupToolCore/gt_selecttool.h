/*
File:    gt_selecttool.h

Desc:    SmtViewCtrlTool,ä¯ÀÀ¹¤¾ß

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_SELECTTOOL_H
#define _GT_SELECTTOOL_H

#include "gt_basetool.h"

namespace Smt_GroupTool
{
	class SmtSelectTool:public SmtBaseTool
	{
	public:
		SmtSelectTool();
		virtual ~SmtSelectTool();

		int					Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int					AuxDraw();
		int					Timer();

	public:
		int					KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	public:
		int					Notify(long nMsg,SmtListenerMsg &param);

	protected:
		void				OnRetDelegate(int nRetType);
		void				OnSetSelMode(void);

	protected:
		lPoint				m_pntOrigin;
		lPoint				m_pntCur;
		lPoint				m_pntPrev;
		bool                m_bCaptured;
		int                 m_nStep;

	protected:
		eSelectMode			m_selMode;
		int					m_nLayerFeaType;

		SmtVectorLayer		*m_pResultLayer;
		SmtGQueryDesc		m_gQDes;
		SmtPQueryDesc		m_pQDes;
		double				m_dpMargin;
	};
}

#endif //_GT_SELECTTOOL_H
