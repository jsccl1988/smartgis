/*
File:    gt_basetool.h

Desc:    SmtBaseTool,���߻���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _GT_BASETOOL_H
#define _GT_BASETOOL_H

#include "base/core/core.h"
#include "base/core/msg.h"
#include "legacy/render/bridge/renderdevice.h"
#include "algorithm/geo/geometry.h"
#include "sdb/map/map.h"
#include "legacy/tool/group/defs.h"

#include "legacy/tool/t_msg.h"
#include "legacy/tool/t_iatool.h"
#include "tool/gestures.h"

using namespace sdb;
using namespace render;
using namespace base;
using namespace tool;

#define SMT_IATOOL_MSG_KEY(lMsg)			(SMT_MSG_KEY(lMsg,m_hWnd))

#define SMT_IATOOL_APPEND_MSG(lMsg)\
{\
	append_msg(SMT_IATOOL_MSG_KEY(lMsg));\
}

namespace tool
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

		// Workspace owns the Interaction; leftover applies the completed draft.
		virtual void			apply_draft(const tool::Draft& draft) { (void)draft; }

	protected:
		LPRENDERDEVICE			m_pRenderDevice;
		SmtMap					*m_pOperMap;

		char                    m_szStyleName[MAX_STYLENAME_LENGTH];
		
		double					m_fScaleDelt;
	};
}

#endif //_GT_BASETOOL_H