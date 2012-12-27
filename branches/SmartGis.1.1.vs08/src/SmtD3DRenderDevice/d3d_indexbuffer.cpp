#include "d3d_indexbuffer.h"

namespace Smt_3Drd
{
	SmtD3DIndexBuffer::SmtD3DIndexBuffer(LPDIRECT3DDEVICE9 pD3DDevice, int count ): SmtIndexBuffer()
	{
		ulong size = 0;

		m_pD3DDevice       = pD3DDevice;
		m_pD3DIndexBuffer = NULL;

		m_dwIndexCount = count;

		m_bLocked     = false;
		m_pIndex   = NULL;  
		
		// Allocate memory for vertex data
		//--
		m_dwStrideIndex = sizeof(uint);
		m_pD3DIndex = new uint[m_dwIndexCount];

		if( FAILED( m_pD3DDevice->CreateIndexBuffer(m_dwStrideIndex*m_dwIndexCount,
			0, D3DFMT_INDEX32,  //索引类型
			D3DPOOL_DEFAULT, &m_pD3DIndexBuffer,NULL) ) )
		{
			;
		} 
	}

	SmtD3DIndexBuffer::~SmtD3DIndexBuffer()
	{
		if ( m_pD3DIndex != NULL )
		{
			delete [] m_pD3DIndex;
			m_pD3DIndex = NULL;
		}
	}

	long SmtD3DIndexBuffer::Lock()
	{
		m_bLocked = true;
		m_pIndex   = m_pD3DIndex; 

		VOID* pIndices;
		if( FAILED( m_pD3DIndexBuffer->Lock( 0, sizeof(m_pIndex), (void**)&pIndices, 0 ) ) )
			return SMT_ERR_FAILURE;

		memcpy( pIndices, m_pIndex, sizeof(m_pIndex) );

		return SMT_ERR_NONE;
	}

	long SmtD3DIndexBuffer::Unlock()
	{
		m_bLocked = false;
		m_pIndex   = NULL;  

		m_pD3DIndexBuffer->Unlock(); 

		return SMT_ERR_NONE;
	}

	void* SmtD3DIndexBuffer::GetIndexData()
	{
		if (NULL == m_pD3DIndex)
			return NULL;

		return (void*)m_pD3DIndex;
	}

	void SmtD3DIndexBuffer::Index(uint index )
	{
		*m_pIndex = index;
		m_pIndex ++;
	}

	long SmtD3DIndexBuffer::PrepareForDrawing()
	{
		if (m_pD3DDevice && m_pD3DIndexBuffer)
		{
			m_pD3DDevice->SetIndices( m_pD3DIndexBuffer );  //设置索引缓冲区 
		}

		return SMT_ERR_NONE;
	}

	long SmtD3DIndexBuffer::EndDrawing()
	{
		if (m_pD3DDevice && m_pD3DIndexBuffer)
		{
			m_pD3DDevice->SetIndices( NULL );  //设置索引缓冲区 
		}

		return SMT_ERR_NONE;
	}
}