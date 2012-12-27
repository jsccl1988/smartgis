/*
File:    gl_indexbuffer.h 

Desc:    SmtGLIndexBuffer,  opengl VertexBuffer Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GLINDEXBUFFER_H
#define _GLINDEXBUFFER_H

#include "rd3d_indexbuffer.h "
#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLIndexBuffer : public SmtIndexBuffer
	{
	protected:
		SmtGLIndexBuffer();         
		SmtGLIndexBuffer( const SmtGLIndexBuffer& );   
		SmtGLIndexBuffer& operator =( const SmtGLIndexBuffer& ); 

	public:
		SmtGLIndexBuffer( int count);
		~SmtGLIndexBuffer();

		long					PrepareForDrawing();
		long					EndDrawing();

		long					Lock();
		long					Unlock();
		bool					IsLocked() const { return m_bLocked; }

		void*					GetIndexData();
		ulong					GetIndexCount() const { return m_dwIndexCount; }
		ulong					GetIndexStride() const { return m_dwStrideIndex; }

		void					Index(uint index);

	private: 
		bool					m_bLocked;						// flag to specify if buffer is locked        

		ulong					m_dwIndexCount;					// number of vertex in buffer
		ulong					m_dwStrideIndex;				// stride of entire index data
		uint					*m_pIndex;						// pointer to head of current vertex

		//----------------------------------------------------------------------
		// OpenGL related members
		//--
		uint					*m_pGLIndex;					// Buffer containing index data
	};
}

#endif //_GLINDEXBUFFER_H