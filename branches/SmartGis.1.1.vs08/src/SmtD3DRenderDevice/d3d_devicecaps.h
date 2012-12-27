/*
File:    d3d_3ddevicecaps.h

Desc:    SmtD3DDeviceCaps,ÏÔ¿¨ÄÜÁ¦

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3D_3DDEVICECAPS_H
#define _D3D_3DDEVICECAPS_H

#include "rd3d_3ddevicecaps.h"
#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SmtD3DDeviceCaps : public Smt3DDeviceCaps
	{
	public:
		SmtD3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice,IDirect3D9	*pD3D);
		virtual ~SmtD3DDeviceCaps(void){};

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

	private:
		D3DCAPS9				m_D3DCaps;

	};
}

#endif //_D3D_3DDEVICECAPS_H

