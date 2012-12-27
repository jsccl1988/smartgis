/*
File:    rd3d_framebuffer.h

Desc:    SmtFrameBuffer, FrameBuffer ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_FRAMEBUFFER_H
#define _RD3D_FRAMEBUFFER_H

#include "rd3d_3drenderdefs.h"
#include "rd3d_renderbuffer.h"

namespace Smt_3Drd
{
	enum RenderBufferSlot
	{
		COLOR_ATTACHMENT0 = 0,
		COLOR_ATTACHMENT1,
		COLOR_ATTACHMENT2,
		COLOR_ATTACHMENT3,
		COLOR_ATTACHMENT4,
		COLOR_ATTACHMENT5,
		COLOR_ATTACHMENT6,
		COLOR_ATTACHMENT7,
		DEPTH_ATTACHMENT,
		STENCIL_ATTACHMENT
	};

	enum FrameBufferStatus
	{
		FRAMEBUFFER_COMPLETE = 0,
		FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
		FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT,
		FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER,
		FRAMEBUFFER_INCOMPLETE_READ_BUFFER,
		FRAMEBUFFER_UNSUPPORTED
	};

	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtFrameBuffer
	{
	public:
		SmtFrameBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle);
		virtual ~SmtFrameBuffer();

	public:
		inline uint				GetHandle() {return m_unHandle;}
		
		long					Use();
		long					Unuse();

		FrameBufferStatus		CheckStatus();
		long					AttachRenderBuffer(SmtRenderBuffer *renderBuffer, RenderBufferSlot slot);
		long					AttachTexture2D(SmtTexture *texture2D, RenderBufferSlot slot);

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		uint					m_unHandle;
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_FRAMEBUFFER_H