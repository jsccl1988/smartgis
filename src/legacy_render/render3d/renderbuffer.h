/*
File:    rd3d_renderbuffer.h

Desc:    SmtRenderBuffer, RenderBuffer ����ӿ�

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_RENDERBUFFER_H
#define _RD3D_RENDERBUFFER_H

#include "legacy_render/render3d/base.h"
#include "legacy_render/render3d/texture.h"

namespace render
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class RENDER3D_EXPORT_CLASS SmtRenderBuffer
	{
	public:
		SmtRenderBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle, TextureFormat format, uint width, uint height):m_p3DRenderDevice(p3DRenderDevice)
			,m_unHandle(height)
			,m_texFormat(format)
			,m_unWidth(width)
			,m_unHeight(height)
		{
			;
		}

		virtual ~SmtRenderBuffer()
		{

		}

	public:
		inline uint				GetHandle(void) const {return m_unHandle;}

		inline uint				GetWidth(void) const {return m_unWidth;}
		inline uint				GetHeight(void) const {return m_unHeight;}
		inline TextureFormat	GetFormat(void) const {return m_texFormat;}

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		uint					m_unHandle;

		uint					m_unWidth;
		uint					m_unHeight;
		TextureFormat			m_texFormat;
	};
}

#if !defined(RENDER3D_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"legacy_render_d.lib")
#       else
#          pragma comment(lib,"legacy_render.lib")
#	    endif
#endif

#endif //_RD3D_RENDERBUFFER_H