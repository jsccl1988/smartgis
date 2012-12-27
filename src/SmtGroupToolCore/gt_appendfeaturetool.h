/*
File:    gt_appendfeaturetool.h

Desc:    SmtAppendFeatureTool,添加要素工具

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_APPEND_FEATURE_H
#define _GT_APPEND_FEATURE_H

#include "gt_basetool.h"
#include "smt_command.h"
using namespace  Smt_Core;

namespace Smt_GroupTool
{
	class SmtAppendFeatureTool:public SmtBaseTool
	{
	public:
		SmtAppendFeatureTool();
		virtual ~SmtAppendFeatureTool();
		int                      Create() ;
		int                      Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);
		int                      AuxDraw();
		int                      Timer();

	public:
		inline virtual void		 SetOperMap(SmtMap *pOperSmtMap);

	public:
		int                      Notify(long nMsg,SmtListenerMsg &param);

	public:
		int						 KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	protected:
		void					 OnRetDelegate(int nRetType);

	protected:
		void					 OnInputPointFeature(ushort unType);
		void					 OnInputLineFeature(ushort unType);
		void					 OnInputRegionFeature(ushort unType);

	protected:
		void					 AppendPointFeature(ushort unType);
		void                     AppendChildImageFeature();
		void                     AppendTextFeature(const char * szAnno,float fangle);
		void                     AppendDotFeature();

		void					 AppendLineFeature(void);
		void					 AppendRegionFeature(void);

	protected:
		SmtGeometry				 *m_pGeom;
		string					 m_strAnno;

	protected:
		SmtCommandManager		 m_cmdMgr;
	};
}

#endif //_GT_APPEND_FEATURE_H
