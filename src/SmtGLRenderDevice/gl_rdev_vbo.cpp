#include "gl_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//video buffer
	SmtVideoBuffer* SmtGLRenderDevice::CreateVideoBuffer(ArrayType type)
	{
		GLhandleARB newHandle;
		m_pFuncVBO->glGenBuffers(1, &newHandle);
		SmtVideoBuffer *newVideoBuffer = new SmtVideoBuffer(this, newHandle, type);
		
		return newVideoBuffer;
	}

	long SmtGLRenderDevice::BindBuffer(SmtVideoBuffer *buffer)
	{
		GLhandleARB handle = buffer->GetHandle();
		m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, handle);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::BindIndexBuffer(SmtVideoBuffer *buffer)
	{
		GLhandleARB handle = buffer->GetHandle();
		m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::UnbindBuffer()
	{
		m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, 0);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::UnbindIndexBuffer()
	{
		m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::UpdateBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;
	
		BindBuffer(buffer);
		GLenum GLMethod = ConvertVideoBufferStoreMethod(method);
		m_pFuncVBO->glBufferData(GL_ARRAY_BUFFER, size, data, GLMethod);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::UpdateIndexBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method)
	{
		if (NULL == buffer || NULL == data)
			return SMT_ERR_INVALID_PARAM;

		BindIndexBuffer(buffer);
	
		GLenum GLMethod = ConvertVideoBufferStoreMethod(method);
		m_pFuncVBO->glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GLMethod);

		return SMT_ERR_NONE;
	}

	void *SmtGLRenderDevice::MapBuffer(SmtVideoBuffer* buffer, AccessMode access)
	{
		if (NULL == buffer)
			return NULL;

		GLhandleARB handle = buffer->GetHandle();
		GLenum glAccess = ConvertAccess(access);
	
		void *result = m_pFuncVBO->glMapBuffer(GL_ARRAY_BUFFER, glAccess);
		
		return result;
	}

	long SmtGLRenderDevice::UnmapBuffer(SmtVideoBuffer *buffer)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;

		GLboolean result = m_pFuncVBO->glUnmapBuffer(GL_ARRAY_BUFFER);
		
		return SMT_ERR_NONE;
	}

	void* SmtGLRenderDevice::MapIndexBuffer(SmtVideoBuffer* buffer, AccessMode access)
	{
		if (NULL == buffer)
			return NULL;

		GLhandleARB handle = buffer->GetHandle();
		GLenum glAccess = ConvertAccess(access);

		void *result = m_pFuncVBO->glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, glAccess);
		
		return result;
	}

	long SmtGLRenderDevice::UnmapIndexBuffer(SmtVideoBuffer *buffer)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;

		GLboolean result = m_pFuncVBO->glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::DestroyBuffer(SmtVideoBuffer *buffer)
	{
		GLhandleARB handle = buffer->GetHandle();
		m_pFuncVBO->glDeleteBuffers(1, &handle);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::DestroyIndexBuffer(SmtVideoBuffer *buffer)
	{
		GLhandleARB handle = buffer->GetHandle();
		m_pFuncVBO->glDeleteBuffers(1, &handle);

		return SMT_ERR_NONE;
	}
}