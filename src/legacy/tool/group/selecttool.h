/*
File:    gt_selecttool.h

Desc:    SmtViewCtrlTool,�������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_SELECTTOOL_H
#define _GT_SELECTTOOL_H

#include "legacy/tool/group/basetool.h"
#include "sdb/datasource/mgr/datasourcemgr.h"
#include "tool/gestures.h"

namespace tool
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
		int					notify(long nMsg,SmtListenerMsg &param);
		void				apply_draft(const tool::Draft& draft) override;

	protected:
		void				OnRetDelegate(int nRetType);
		void				OnSetSelMode(void);

	protected:
		eSelectMode			m_selMode;
		int					m_nLayerFeaType;

		sdb::ScratchLayer m_resultLayer;
		SmtGQueryDesc		m_gQDes;
		SmtPQueryDesc		m_pQDes;
		double				m_dpMargin;
	};
}

#endif //_GT_SELECTTOOL_H
