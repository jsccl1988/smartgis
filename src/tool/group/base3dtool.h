/*
File:    gt_base3dtool.h

Desc:    SmtBase3DTool,3D���߻���


Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_BASE3DTOOL_H
#define _GT_BASE3DTOOL_H

#include "base/core/core.h"
#include "base/core/msg.h"
#include "base/core/bas_struct.h"
#include "render/render3d/3drenderdevice.h"
#include "tool/group/defs.h"
#include "tool/group/basetool.h"
#include "render/scene3d/bl3d_scene.h"

#include "tool/t_msg.h"
#include "tool/t_iatool.h"
#include "tool/gestures.h"

using namespace render;
using namespace base;
using namespace tool;

namespace tool
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

		virtual void			apply_draft(const tool::Draft& draft) { (void)draft; }

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		SmtScene				*m_pScene;
	};
}

#endif //_GT_BASE3DTOOL_H