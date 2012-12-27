/*
File:    gt_base3dtool.h

Desc:    SmtBase3DTool,3D工具基类


Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_BASE3DTOOL_H
#define _GT_BASE3DTOOL_H

#include "smt_core.h"
#include "smt_msg.h"
#include "smt_bas_struct.h"
#include "rd3d_3drenderdevice.h"
#include "gt_defs.h"
#include "gt_basetool.h"
#include "bl3d_scene.h"

#include "t_msg.h"
#include "t_iatool.h"

using namespace Smt_3Drd;
using namespace Smt_Core;
using namespace Smt_IATool;

namespace Smt_GroupTool
{
	class  SmtBase3DTool  :public SmtIATool 
	{
	public:
		SmtBase3DTool();
		virtual ~SmtBase3DTool();

	public:
		virtual int             Init(LP3DRENDERDEVICE p3DRenderDevice,SmtScene *pScene,HWND hWnd,pfnToolCallBack pfnCallBack = NULL,void* pToFollow = NULL);

	public:
		LP3DRENDERDEVICE		GetRenderDevice(void) {return m_p3DRenderDevice;}
		SmtScene				*GetScene(void){return m_pScene;}

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		SmtScene				*m_pScene;
	};
}

#endif //_GT_BASE3DTOOL_H