#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//video buffer
	SmtVideoBuffer* SmtD3DRenderDevice::CreateVideoBuffer(ArrayType type)
	{
		uint newHandle;
		m_pFuncVBO->glGenBuffers(1, &newHandle);
		SmtVideoBuffer *newVideoBuffer = new SmtVideoBuffer(this, newHandle, type);
		
		return newVideoBuffer;
	}

	long SmtD3DRenderDevice::BindBuffer(SmtVideoBuffer *buffer)
	{
		uint handle = buffer->GetHandle();
		//m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, handle);
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::BindIndexBuffer(SmtVideoBuffer *buffer)
	{
		uint handle = buffer->GetHandle();
		//m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UnbindBuffer()
	{
		//m_pFuncVBO->glBindBuffer(GL_ARRAY_BUFFER, 0);
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UnbindIndexBuffer()
	{
		//m_pFuncVBO->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UpdateBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;
	
		BindBuffer(buffer);
		uint GLMethod = ConvertVideoBufferStoreMethod(method);
		//m_pFuncVBO->glBufferData(GL_ARRAY_BUFFER, size, data, GLMethod);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::UpdateIndexBuffer(SmtVideoBuffer* buffer, void *data, uint size, VideoBufferStoreMethod method)
	{
		if (NULL == buffer || NULL == data)
			return SMT_ERR_INVALID_PARAM;

		BindIndexBuffer(buffer);
	
		uint GLMethod = ConvertVideoBufferStoreMethod(method);
		//m_pFuncVBO->glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GLMethod);

		return SMT_ERR_NONE;
	}

	void *SmtD3DRenderDevice::MapBuffer(SmtVideoBuffer* buffer, AccessMode access)
	{
		if (NULL == buffer)
			return NULL;

		uint handle = buffer->GetHandle();
		uint glAccess = ConvertAccess(access);
	
		//void *result = m_pFuncVBO->glMapBuffer(GL_ARRAY_BUFFER, glAccess);
		
		//return result;
		return NULL;
	}

	long SmtD3DRenderDevice::UnmapBuffer(SmtVideoBuffer *buffer)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;

		//uchar result = m_pFuncVBO->glUnmapBuffer(GL_ARRAY_BUFFER);
		
		return SMT_ERR_NONE;
	}

	void* SmtD3DRenderDevice::MapIndexBuffer(SmtVideoBuffer* buffer, AccessMode access)
	{
		if (NULL == buffer)
			return NULL;

		uint handle = buffer->GetHandle();
		uint glAccess = ConvertAccess(access);

		//void *result = m_pFuncVBO->glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, glAccess);
		
		//return result;
		return NULL;
	}

	long SmtD3DRenderDevice::UnmapIndexBuffer(SmtVideoBuffer *buffer)
	{
		if (NULL == buffer)
			return SMT_ERR_INVALID_PARAM;

		//uchar result = m_pFuncVBO->glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::DestroyBuffer(SmtVideoBuffer *buffer)
	{
		uint handle = buffer->GetHandle();
		m_pFuncVBO->glDeleteBuffers(1, &handle);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::DestroyIndexBuffer(SmtVideoBuffer *buffer)
	{
		uint handle = buffer->GetHandle();
		m_pFuncVBO->glDeleteBuffers(1, &handle);

		return SMT_ERR_NONE;
	}
}