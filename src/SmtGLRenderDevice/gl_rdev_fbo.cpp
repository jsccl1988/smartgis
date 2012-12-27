#include "gl_3drenderdevice.h"
#include "smt_logmanager.h"
using namespace Smt_Core;

namespace Smt_3Drd
{
	SmtFrameBuffer* SmtGLRenderDevice::CreateFrameBuffer()
	{
		GLhandleARB handle;
		m_pFuncFBO->glGenFramebuffers(1, &handle);
		SmtFrameBuffer *newFB = new SmtFrameBuffer(this, handle);

		return newFB;
	}

	long SmtGLRenderDevice::DestroyFrameBuffer(SmtFrameBuffer *frameBuffer)
	{
		if (frameBuffer)
		{
			GLhandleARB handle = frameBuffer->GetHandle();
			m_pFuncFBO->glDeleteFramebuffers(1, &handle);
		}
		
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::BindFrameBuffer(SmtFrameBuffer *frameBuffer)
	{
		if (frameBuffer)
		{
			GLhandleARB handle = frameBuffer->GetHandle();
			m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT, handle);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::UnbindFrameBuffer()
	{
		m_pFuncFBO->glBindFramebuffer(GL_FRAMEBUFFER_EXT,0 );
		return SMT_ERR_NONE;
	}

	SmtRenderBuffer* SmtGLRenderDevice::CreateRenderBuffer(TextureFormat format, uint width, uint height)
	{
		
		GLhandleARB handle;
		GLenum rbFormat = ConvertTexFormat(format);
		
		m_pFuncFBO->glGenRenderbuffers(1, &handle);
		m_pFuncFBO->glBindRenderbuffer(GL_RENDERBUFFER_EXT, handle);
		m_pFuncFBO->glRenderbufferStorage(GL_RENDERBUFFER_EXT, rbFormat, width, height);
		
		SmtRenderBuffer *pRenderBuf = new SmtRenderBuffer(this, handle, format, width, height);

		return pRenderBuf;
	}

	long SmtGLRenderDevice::DestroyRenderBuffer(SmtRenderBuffer *renderBuffer)
	{
		GLhandleARB handle = renderBuffer->GetHandle();
		m_pFuncFBO->glDeleteRenderbuffers(1, &handle);
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::AttachRenderBuffer(SmtFrameBuffer *frameBuffer, SmtRenderBuffer *renderBuffer, RenderBufferSlot slot)
	{
		GLhandleARB frmHandle = frameBuffer->GetHandle();
		GLhandleARB rdhandle = renderBuffer->GetHandle();

		GLenum rbSlot = ConvertRenderBufferSlot(slot);
		m_pFuncFBO->glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, rbSlot, GL_RENDERBUFFER_EXT, rdhandle);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::AttachTexture(SmtFrameBuffer *frameBuffer, SmtTexture *texture2D, RenderBufferSlot slot)
	{
		GLenum rbSlot = ConvertRenderBufferSlot(slot);
		GLhandleARB id = texture2D->GetHandle();
		m_pFuncFBO->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, rbSlot, GL_TEXTURE_2D, id, 0);
		return SMT_ERR_NONE;
	}

	FrameBufferStatus SmtGLRenderDevice::CheckFrameBufferStatus()
	{
		GLenum result = m_pFuncFBO->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
		switch (result)
		{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return FRAMEBUFFER_COMPLETE;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			return FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			return FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			return FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			return FRAMEBUFFER_INCOMPLETE_READ_BUFFER;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			return FRAMEBUFFER_UNSUPPORTED;
		default:
			return FRAMEBUFFER_UNSUPPORTED;
		}
	}

	GLenum SmtGLRenderDevice::ConvertRenderBufferSlot(RenderBufferSlot slot)
	{
		GLenum map[] =
		{
			GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT,
			GL_COLOR_ATTACHMENT3_EXT, GL_COLOR_ATTACHMENT4_EXT, GL_COLOR_ATTACHMENT5_EXT,
			GL_COLOR_ATTACHMENT6_EXT, GL_COLOR_ATTACHMENT7_EXT, GL_DEPTH_ATTACHMENT_EXT,
			GL_STENCIL_ATTACHMENT_EXT
		};

		int index = static_cast<int>(slot);
		if (index > 0 && index <= sizeof(map) / sizeof(GLenum))
			return map[index];
		else
			return -1;	
	}
}