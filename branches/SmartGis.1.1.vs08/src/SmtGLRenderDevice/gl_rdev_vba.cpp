#include "gl_3drenderdevice.h"
#include "gl_vertexbuffer.h"
#include "gl_indexbuffer.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	SmtVertexBuffer*SmtGLRenderDevice::CreateVertexBuffer( int nCount, DWORD dwFormat, bool bDynamic)
	{
		SmtGLVertexBuffer *pVB = new SmtGLVertexBuffer( nCount, dwFormat, bDynamic );

		return pVB;
	}

	SmtIndexBuffer* SmtGLRenderDevice::CreateIndexBuffer(int nCount)
	{
		SmtGLIndexBuffer * pIndex = new SmtGLIndexBuffer(nCount);

		return pIndex;
	}

	//vba
	long SmtGLRenderDevice::SetVertexArray(int components, Type type, int stride, void* data)
	{
		glVertexPointer(components, ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetTextureCoordsArray(int components, Type type, int stride, void* data)
	{
		glTexCoordPointer(components, ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetNormalArray(Type type, int stride, void* data)
	{
		glNormalPointer(ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetIndexArray(Type type, int stride, void* data)
	{
		glIndexPointer(ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::EnableArray(ArrayType type, bool enabled)
	{
		if (enabled == true)
		{
			glEnableClientState(ConvertArrayType(type));
		}
		else
		{
			glDisableClientState(ConvertArrayType(type));
		}
		
		return SMT_ERR_NONE;
	}
}