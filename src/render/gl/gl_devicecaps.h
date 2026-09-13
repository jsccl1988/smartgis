/*
File:    gl_3ddevicecaps.h

Desc:    SmtGLDeviceCaps,�Կ�����

Version: Version 1.0

Writter:  �´���

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GL_3DDEVICECAPS_H
#define _GL_3DDEVICECAPS_H

#include "render/render3d/3ddevicecaps.h"

namespace render
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SmtGLDeviceCaps : public Smt3DDeviceCaps
	{
	public:
		SmtGLDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice);
		virtual ~SmtGLDeviceCaps(void){};

	public:
		virtual bool			IsVBOSupported();
		virtual bool			IsMipMapsSupported();
		virtual bool			IsFBOSupported();
		virtual bool			IsGLSLSupported();
		virtual bool			IsVSyncSupported();
		virtual bool			IsMultiTextureSupported();
		virtual bool			IsAnisotropySupported();
		virtual int				GetTextureSlotsCount();
		virtual int				GetMaxColorAttachments();
		virtual float			GetMaxAnisotropy();

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
	};
}

#endif //_GL_3DDEVICECAPS_H

