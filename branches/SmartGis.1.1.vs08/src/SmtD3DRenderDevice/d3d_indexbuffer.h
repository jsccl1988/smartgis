/*
File:    d3d_indexbuffer.h 

Desc:    SmtD3DIndexBuffer,  D3D VertexBuffer Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3DINDEXBUFFER_H
#define _D3DINDEXBUFFER_H

#include "rd3d_indexbuffer.h "
#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DIndexBuffer : public SmtIndexBuffer
	{
	protected:
		SmtD3DIndexBuffer();         
		SmtD3DIndexBuffer( const SmtD3DIndexBuffer& );   
		SmtD3DIndexBuffer& operator =( const SmtD3DIndexBuffer& ); 

	public:
		SmtD3DIndexBuffer(LPDIRECT3DDEVICE9 pD3DDevice, int count);
		~SmtD3DIndexBuffer();

		long					PrepareForDrawing();
		long					EndDrawing();

		long					Lock();
		long					Unlock();
		bool					IsLocked() const { return m_bLocked; }

		void*					GetIndexData();
		ulong					GetIndexCount() const { return m_dwIndexCount; }
		ulong					GetIndexStride() const { return m_dwStrideIndex; }

		void					Index(uint index);

	protected:
		const LPDIRECT3DINDEXBUFFER9 GetD3DIndexBuffer() const { return m_pD3DIndexBuffer; }

	private: 
		bool					m_bLocked;						// flag to specify if buffer is locked        

		ulong					m_dwIndexCount;					// number of vertex in buffer
		ulong					m_dwStrideIndex;				// stride of entire index data
		uint					*m_pIndex;						// pointer to head of current vertex

		//----------------------------------------------------------------------
		// D3D related members
		//--
		uint					*m_pD3DIndex;					// Buffer containing index data
		
		LPDIRECT3DINDEXBUFFER9	m_pD3DIndexBuffer;		// d3d index buffer
		LPDIRECT3DDEVICE9       m_pD3DDevice;			// d3d device
	};
}

#endif //_D3DINDEXBUFFER_H