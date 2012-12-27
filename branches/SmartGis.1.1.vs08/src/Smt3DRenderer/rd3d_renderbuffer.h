/*
File:    rd3d_renderbuffer.h

Desc:    SmtRenderBuffer, RenderBuffer ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_RENDERBUFFER_H
#define _RD3D_RENDERBUFFER_H

#include "rd3d_base.h"
#include "rd3d_texture.h"

namespace Smt_3Drd
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtRenderBuffer
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

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_RENDERBUFFER_H