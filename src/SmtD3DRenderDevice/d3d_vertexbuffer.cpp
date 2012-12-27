#include "d3d_vertexbuffer.h"

namespace Smt_3Drd
{
	SmtD3DVertexBuffer::SmtD3DVertexBuffer(LPDIRECT3DDEVICE9 pD3DDevice, int count, ulong format, bool isDynamic ): SmtVertexBuffer()
	{
		ulong size = 0;

		m_pD3DDevice       = pD3DDevice;
		m_pD3DVertexBuffer = NULL;

		m_FVF          = 0;
		m_dwFormat       = format;
		m_dwVertexCount = count;

		if( format & VF_XYZ )
		{
			m_FVF |= D3DFVF_XYZ;
			size += sizeof( float ) * 3;
			m_dwOffsetVertex = 0;
		}
		else if( format & VF_XYZRHW )
		{
			m_FVF |= D3DFVF_XYZRHW;
			size += sizeof( float ) * 4;
			m_dwOffsetVertex = 0;
		}

		if( format & VF_NORMAL )
		{     
			m_FVF |= D3DFVF_NORMAL;
			m_dwOffsetNormal = size;
			size += sizeof( float ) * 3;        
		}

		if( format & VF_DIFFUSE )
		{
			m_FVF |= D3DFVF_DIFFUSE;
			m_dwOffsetDiffuse = size;
			size += sizeof( ulong );
		}

		if( format & VF_TEXCOORD )
		{
			m_FVF |= D3DFVF_TEX1;            
			m_dwOffsetTexCoord = size;
			size += sizeof( float ) * 2;        
		}

		m_dwStrideVertex = size;
		m_bDynamic    = isDynamic;
		m_bLocked     = false;
		m_pVertex      = NULL;
		m_pBuffer	  = NULL;


		HRESULT hRes;

		if (m_bDynamic)
		{
			hRes = m_pD3DDevice->CreateVertexBuffer( 
				m_dwStrideVertex * count, 
				D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
				m_FVF, 
				D3DPOOL_DEFAULT, 
				&m_pD3DVertexBuffer, NULL );
		}
		else
		{
			hRes = m_pD3DDevice->CreateVertexBuffer( 
				m_dwStrideVertex * count, 
				D3DUSAGE_WRITEONLY,
				m_FVF, 
				D3DPOOL_MANAGED, 
				&m_pD3DVertexBuffer, NULL );
		}
	}

	SmtD3DVertexBuffer::~SmtD3DVertexBuffer()
	{
		SMT_SAFE_RELEASE(m_pD3DVertexBuffer);
	}

	long SmtD3DVertexBuffer::Lock()
	{
		HRESULT hRes = m_pD3DVertexBuffer->Lock( 0, 0, &m_pBuffer, (m_bDynamic?D3DLOCK_DISCARD:0) );

		if ( FAILED(hRes) )
			::OutputDebugString( "Unable to lock D3D Vertex Buffer." );

		m_bLocked = true;
		m_pVertex  = (BYTE*)m_pBuffer;

		return (!FAILED(hRes))?SMT_ERR_NONE:SMT_ERR_FAILURE;
	}

	long SmtD3DVertexBuffer::Unlock()
	{
		HRESULT hRes = m_pD3DVertexBuffer->Unlock();

		if ( FAILED(hRes) )
			::OutputDebugString( "Unable to unlock D3D Vertex Buffer." );

		m_bLocked = false;
		m_pVertex  = NULL;
		m_pBuffer = NULL;

		return (!FAILED(hRes))?SMT_ERR_NONE:SMT_ERR_FAILURE;
	}

	void	*SmtD3DVertexBuffer::GetVertexData()
	{
		return m_pBuffer;
	}

	void SmtD3DVertexBuffer::Vertex( float x, float y, float z )
	{
		float* vertex = (float*)(m_pVertex + m_dwOffsetVertex);

		vertex[ 0 ] = x;
		vertex[ 1 ] = y;
		vertex[ 2 ] = z;

		m_pVertex += m_dwStrideVertex;
	}

	void SmtD3DVertexBuffer::Vertex( float x, float y, float z, float w )
	{
		float* vertex = (float*)(m_pVertex + m_dwOffsetVertex);

		vertex[ 0 ] = x;
		vertex[ 1 ] = y;
		vertex[ 2 ] = z;
		vertex[ 3 ] = w;

		m_pVertex += m_dwStrideVertex;
	}

	void SmtD3DVertexBuffer::Normal( float x, float y, float z )
	{
		float* normal = (float*)( m_pVertex + m_dwOffsetNormal );

		normal[ 0 ] = x;
		normal[ 1 ] = y;
		normal[ 2 ] = z;
	}

	void SmtD3DVertexBuffer::Diffuse( float r, float g, float b, float a  )
	{
		ulong* diffuse = (ulong*)( m_pVertex + m_dwOffsetDiffuse );

		*diffuse = D3DCOLOR_COLORVALUE( r, g, b, a );
	}

	void SmtD3DVertexBuffer::TexVertex( float u, float v )
	{
		float* texcoord = (float*)( m_pVertex + m_dwOffsetTexCoord );

		texcoord[ 0 ] = u;
		texcoord[ 1 ] = v;
	}

	long SmtD3DVertexBuffer::PrepareForDrawing()
	{
		HRESULT hRes;

		if( m_FVF & D3DFVF_NORMAL )
		{     
			hRes = m_pD3DDevice->SetRenderState(D3DRS_NORMALIZENORMALS,TRUE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		if( m_FVF & D3DFVF_DIFFUSE )
		{
			hRes = m_pD3DDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,TRUE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		if( m_FVF & D3DFVF_TEX1 )
		{
			hRes = m_pD3DDevice->SetRenderState(D3DRS_TEXTUREFACTOR,TRUE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		hRes = m_pD3DDevice->SetStreamSource( 0, m_pD3DVertexBuffer, 0, m_dwStrideVertex );
		if (FAILED(hRes))
			return SMT_ERR_FAILURE;

		hRes = m_pD3DDevice->SetFVF( m_FVF );
		if (FAILED(hRes))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtD3DVertexBuffer::EndDrawing()
	{
		HRESULT hRes;

		if( m_FVF & D3DFVF_NORMAL )
		{     
			hRes = m_pD3DDevice->SetRenderState(D3DRS_NORMALIZENORMALS,FALSE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		if( m_FVF & D3DFVF_DIFFUSE )
		{
			hRes = m_pD3DDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,FALSE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		if( m_FVF & D3DFVF_TEX1 )
		{
			hRes = m_pD3DDevice->SetRenderState(D3DRS_TEXTUREFACTOR,FALSE);
			if (FAILED(hRes))
				return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}
}