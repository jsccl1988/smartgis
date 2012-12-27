#include "d3d_3drenderdevice.h"
#include "d3d_vertexbuffer.h"
#include "d3d_indexbuffer.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	SmtVertexBuffer*SmtD3DRenderDevice::CreateVertexBuffer( int nCount, DWORD dwFormat, bool bDynamic)
	{
		SmtD3DVertexBuffer *pVB = new SmtD3DVertexBuffer(m_pD3DDevice, nCount, dwFormat, bDynamic );

		return pVB;
	}

	SmtIndexBuffer* SmtD3DRenderDevice::CreateIndexBuffer(int nCount)
	{
		SmtD3DIndexBuffer * pIndex = new SmtD3DIndexBuffer(m_pD3DDevice, nCount);

		return pIndex;
	}

	//vba
	long SmtD3DRenderDevice::SetVertexArray(int components, Type type, int stride, void* data)
	{
		//glVertexPointer(components, ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetTextureCoordsArray(int components, Type type, int stride, void* data)
	{
		//glTexCoordPointer(components, ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetNormalArray(Type type, int stride, void* data)
	{
		//glNormalPointer(ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetIndexArray(Type type, int stride, void* data)
	{
		//glIndexPointer(ConvertType(type), stride, data);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::EnableArray(ArrayType type, bool enabled)
	{
		if (enabled == true)
		{
			//glEnableClientState(ConvertArrayType(type));
		}
		else
		{
			//glDisableClientState(ConvertArrayType(type));
		}
		
		return SMT_ERR_NONE;
	}
}