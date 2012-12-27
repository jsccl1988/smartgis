/*
File:    rd3d_videobuffer.h

Desc:    SmtVertexBuffer, VertexBuffer ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_INDEXBUFFER_H
#define _RD3D_INDEXBUFFER_H
#include "rd3d_3drenderdefs.h"

/**
* This is an abstract class that provides an interface on top of various
* implementations of vertex buffers or similar structures.
*
* Inspired by the work of Tobin Schwaiger-Hastanan
*/
namespace Smt_3Drd
{
	class SmtIndexBuffer
	{
	public:
		virtual ~SmtIndexBuffer() {}

		/**
		* Locks the Index buffer (usually for writing)...      
		* @return True if the lock request succeeded. False else.
		*/
		virtual long		Lock() = 0;

		/**
		* Unlocks the Index buffer.      
		* @return True if the unlock request succeeded. False else.
		*/
		virtual long		Unlock() = 0;

		/**
		* Determines if the Index buffer is locked or not.      
		* @return boolean value specifying if Index buffer is locked or not.
		*/
		virtual bool		IsLocked() const = 0;

		/**
		* Returns a pointer to the raw vertex data.      
		* This should only be called when vertex buffer is locked.      
		* @return pointer to raw Index data.
		*/
		virtual void		*GetIndexData() = 0;

		/**
		* returns the number of vertex currently in the vertex buffer
		* @return number of vertex in vertex buffer.
		*/
		virtual ulong		GetIndexCount() const = 0;

		/**
		* Specifies untransformed Index data. Should be last call
		* for each Index. 
		*/
		virtual void		Index(uint index) = 0;

		/**
		* This method is called to setup the Index buffer just before a call
		* to render primitives
		*/
		virtual long		PrepareForDrawing() = 0;

		/**
		* This method is called to clear the Index buffer just after a call
		* to render primitives
		*/
		virtual long		EndDrawing() = 0;
	};
}

#endif //_RD3D_INDEXBUFFER_H