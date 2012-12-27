/*
File:    d3d_vertexbuffer.h 

Desc:    SmtD3DVertexBuffer, D3D VertexBuffer Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _D3DVERTEXBUFFER_H
#define _D3DVERTEXBUFFER_H

#include "rd3d_vertexbuffer.h "
#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DVertexBuffer : public SmtVertexBuffer
	{
	protected:
		SmtD3DVertexBuffer();         
		SmtD3DVertexBuffer( const SmtD3DVertexBuffer& );   
		SmtD3DVertexBuffer& operator =( const SmtD3DVertexBuffer& ); 

	public:
		SmtD3DVertexBuffer(LPDIRECT3DDEVICE9 pD3DDevice, int count, ulong format,bool isDynamic = true );
		~SmtD3DVertexBuffer();

		long					PrepareForDrawing();
		long					EndDrawing();

		long					Lock();
		long					Unlock();
		bool					IsLocked() const { return m_bLocked; }

		void*					GetVertexData();

		ulong					GetVertexCount() const { return m_dwVertexCount; }
		ulong					GetVertexFormat() const { return m_dwFormat; }
		ulong					GetVertexStride() const { return m_dwStrideVertex; }

		void					Vertex( float x, float y, float z );
		void					Vertex( float x, float y, float z, float w );
		void					Normal( float x, float y, float z );
		void					Diffuse( float r, float g, float b, float a = 1.0f );
		void					TexVertex( float u, float v );

	protected:
		const LPDIRECT3DVERTEXBUFFER9 GetD3DVertexBuffer() const { return m_pD3DVertexBuffer; }
		const ulong				GetD3DVertexFormat() const { return m_FVF; }

	private: 
		bool					m_bLocked;				// flag to specify if buffer is locked        
		bool					m_bDynamic;				// flag to specify if buffer is holding dynamic data
		void					*m_pBuffer;				// pointer to vertex data
		ulong					m_dwVertexCount;		// number of vertex in buffer
		ulong					m_dwFormat;				// vertex format 

		BYTE					*m_pVertex;				// pointer to head of current vertex

		ulong					m_dwStrideVertex;		// stride of entire vertex data
		ulong					m_dwOffsetVertex;		// offset of vertex data start of vertex
		ulong					m_dwOffsetNormal;		// offset of normal data from start of vertex
		ulong					m_dwOffsetDiffuse;		// offset of diffuse data from start of vertex
		ulong					m_dwOffsetTexCoord;		// offset of TexVertex data from start of vertex

		//----------------------------------------------------------------------
		// D3D related members
		//--
		LPDIRECT3DVERTEXBUFFER9 m_pD3DVertexBuffer;		// d3d vertex buffer
		LPDIRECT3DDEVICE9       m_pD3DDevice;			// d3d device
		ulong                   m_FVF;					// d3d vertex format
	};
}

#endif //_D3DVERTEXBUFFER_H