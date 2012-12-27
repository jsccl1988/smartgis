#include "gl_indexbuffer.h"

namespace Smt_3Drd
{
	SmtGLIndexBuffer::SmtGLIndexBuffer( int count ): SmtIndexBuffer()
	{
		ulong size = 0;

		m_dwIndexCount = count;

		m_bLocked     = false;
		m_pIndex   = NULL;  
		m_pGLIndex  = NULL;

		// Allocate memory for vertex data
		//--
		m_dwStrideIndex = sizeof(uint);
		m_pGLIndex = new uint[m_dwIndexCount];
	}

	SmtGLIndexBuffer::~SmtGLIndexBuffer()
	{
		if ( m_pGLIndex != NULL )
		{
			delete [] m_pGLIndex;
			m_pGLIndex = NULL;
		}
	}

	long SmtGLIndexBuffer::Lock()
	{
		m_bLocked = true;
		m_pIndex   = m_pGLIndex; 

		return SMT_ERR_NONE;
	}

	long SmtGLIndexBuffer::Unlock()
	{
		m_bLocked = false;
		m_pIndex   = NULL;  

		return SMT_ERR_NONE;
	}

	void* SmtGLIndexBuffer::GetIndexData()
	{
		if (NULL == m_pGLIndex)
			return NULL;

		return (void*)m_pGLIndex;
	}

	void SmtGLIndexBuffer::Index(uint index )
	{
		*m_pIndex = index;
		m_pIndex ++;
	}

	long SmtGLIndexBuffer::PrepareForDrawing()
	{
		// Set pointers to arrays
		//--
		if ( m_pGLIndex != NULL )
		{
			glEnableClientState(GL_INDEX_ARRAY);
			glIndexPointer( m_dwIndexCount, GL_UNSIGNED_INT,m_pGLIndex );
		}
		else
		{
			glDisableClientState(GL_INDEX_ARRAY);
		}

		return SMT_ERR_NONE;
	}

	long SmtGLIndexBuffer::EndDrawing()
	{
		if ( m_pGLIndex != NULL )
		{
			glDisableClientState(GL_INDEX_ARRAY);
		}
		
		return SMT_ERR_NONE;
	}
}