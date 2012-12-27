/*
File:    gl_vertexbuffer.h 

Desc:    SmtGLVertexBuffer, opengl VertexBuffer Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GLVERTEXBUFFER_H
#define _GLVERTEXBUFFER_H

#include "rd3d_vertexbuffer.h "
#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLVertexBuffer : public SmtVertexBuffer
	{
	protected:
		SmtGLVertexBuffer();         
		SmtGLVertexBuffer( const SmtGLVertexBuffer& );   
		SmtGLVertexBuffer& operator =( const SmtGLVertexBuffer& ); 

	public:
		SmtGLVertexBuffer( int count, ulong format,bool isDynamic = true );
		~SmtGLVertexBuffer();

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

	private: 
		bool					m_bLocked;					// flag to specify if buffer is locked        
		bool					m_bDynamic;					// flag to specify if buffer is holding dynamic data

		ulong					m_dwVertexCount;				// number of vertex in buffer
		ulong					m_dwFormat;						// vertex format 
		ulong					m_dwStrideVertex;				// stride of entire vertex data
		ulong					m_dwVertexCoordNum;				// Number of coordinates per vertex

		float					*m_pVertex;						// pointer to head of current vertex
		float					*m_pColor;						// pointer to head of current color
		float					*m_pNormal;						// pointer to head of current normal
		float					*m_pTexCoord;					// pointer to head of current texture coordinate

		//----------------------------------------------------------------------
		// OpenGL related members
		//--
		float					*m_pGLVertices;					// Buffer containing vertex data
		float					*m_pGLColors;					// Buffer containing vertex data
		float					*m_pGLNormals;					// Buffer containing vertex data
		float					*m_pGLTexCoords;				// Buffer containing vertex data
	};
}

#endif //_GLVERTEXBUFFER_H