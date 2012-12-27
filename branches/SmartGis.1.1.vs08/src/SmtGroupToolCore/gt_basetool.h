/*
File:    gt_basetool.h

Desc:    SmtBaseTool,工具基类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _GT_BASETOOL_H
#define _GT_BASETOOL_H

#include "smt_core.h"
#include "smt_msg.h"
#include "rd_renderdevice.h"
#include "geo_geometry.h"
#include "gis_map.h"
#include "gt_defs.h"

#include "t_msg.h"
#include "t_iatool.h"

using namespace Smt_GIS;
using namespace Smt_Rd;
using namespace Smt_Core;
using namespace Smt_IATool;

#define SMT_IATOOL_MSG_KEY(lMsg)			(SMT_MSG_KEY(lMsg,m_hWnd))

#define SMT_IATOOL_APPEND_MSG(lMsg)\
{\
	AppendMsg(SMT_IATOOL_MSG_KEY(lMsg));\
}

namespace Smt_GroupTool
{
	extern HINSTANCE g_hInstance;

	//////////////////////////////////////////////////////////////////////////
	class  SmtBaseTool :public SmtIATool
	{
	public:
		SmtBaseTool();
		virtual ~SmtBaseTool();

	public:
		virtual int             Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);

		LPRENDERDEVICE			GetRenderDevice(void) {return m_pRenderDevice;}
		
		virtual void			SetOperMap(SmtMap *pOperSmtMap){ m_pOperMap = pOperSmtMap;}
		virtual void			GetOperMap(SmtMap *&pOperSmtMap){ pOperSmtMap =  m_pOperMap;}

		inline void             SetToolStyleName(const char * name) {strcpy(m_szStyleName,name);}
	    inline const char *     GetToolStyleName(void) {return m_szStyleName;}

	public:
		virtual int				KeyDown(uint nChar, uint nRepCnt, uint nFlags);

	protected:
		LPRENDERDEVICE			m_pRenderDevice;
		SmtMap					*m_pOperMap;

		char                    m_szStyleName[MAX_STYLENAME_LENGTH];
		
		double					m_fScaleDelt;
	};
}

#endif //_GT_BASETOOL_H