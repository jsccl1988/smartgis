/*
File:    rd3d_vertexbuffer.h

Desc:    SmtVertexBuffer, VertexBuffer ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_VTERTEXBUFFER_H
#define _RD3D_VTERTEXBUFFER_H
#include "rd3d_3drenderdefs.h"

/**
* This is an abstract class that provides an interface on top of various
* implementations of vertex buffers or similar structures.
*
* Inspired by the work of Tobin Schwaiger-Hastanan
*/
namespace Smt_3Drd
{
	class SmtVertexBuffer
	{
	public:
		virtual ~SmtVertexBuffer() {}

		/**
		* Locks the vertex buffer (usually for writing)...      
		* @return True if the lock request succeeded. False else.
		*/
		virtual long		Lock() = 0;

		/**
		* Unlocks the vertex buffer.      
		* @return True if the unlock request succeeded. False else.
		*/
		virtual long		Unlock() = 0;

		/**
		* Determines if the vertex buffer is locked or not.      
		* @return boolean value specifying if vertex buffer is locked or not.
		*/
		virtual bool		IsLocked() const = 0;

		/**
		* Returns a pointer to the raw vertex data.      
		* This should only be called when vertex buffer is locked.      
		* @return pointer to raw vertex data.
		*/
		virtual void		*GetVertexData() = 0;

		/**
		* returns the number of vertex currently in the vertex buffer
		* @return number of vertex in vertex buffer.
		*/
		virtual ulong		GetVertexCount() const = 0;

		/**
		* returns the vertex format of the vertex buffer. (see _VERTEXFORMATFLAGS type)
		* @return vertex format of the vertex buffer. 
		*/
		virtual ulong		GetVertexFormat() const = 0;

		/**
		* returns the width in bytes of a single vertex described by the format.
		* @return width in bytes of a single vertex described by the format.
		*/
		virtual ulong		GetVertexStride() const = 0;

		/**
		* Specifies untransformed vertex data. Should be last call
		* for each vertex. That is after the normal, color, and
		* texture coordinate calls are made for the vertex.
		* Note: should be used when format includes FORMAT_XYZ
		*
		* @param x x coordinate.
		* @param y y coordinate.
		* @param z z coordinate.
		*/
		virtual void		Vertex( float x, float y, float z ) = 0;

		/**
		* Specifies transformed vertex data. Should be last call
		* for each vertex. That is after the normal, color, and
		* texture coordinate calls are made for the vertex.
		* Note: should be used when format includes FORMAT_XYZRHW
		*
		* @param x x coordinate.
		* @param y y coordinate.
		* @param z z coordinate.
		* @param w w coordinate.
		*/
		virtual void		Vertex( float x, float y, float z, float w ) = 0;

		/**
		* Specifies vertex normal data.
		* Note: should be used when format includes FORMAT_NORMAL
		*
		* @param x Normal x value.
		* @param y Normal y value.
		* @param z Normal z value.
		*/
		virtual void		Normal( float x, float y, float z ) = 0;

		/**
		* Specifies diffuse color data.
		* Note: should be used when format includes FORMAT_DIFFUSE
		*
		* @param r Red color value.
		* @param g Green color value.
		* @param b Blue color value.
		* @param a Alpha color value.
		*/
		virtual void		Diffuse( float r, float g, float b, float a = 1.0f )  = 0;

		/**
		* Specifies texture vertex. Successive calls should be made to this
		* method if more than one set of texture coordinates need to be 
		* specified.
		* Note: should be used when format includes FORMAT_TEXTUREFLAG
		*
		* @param u U texture coordinate.
		* @param v V texture coordinate.
		*/
		virtual void		TexVertex( float u, float v ) = 0;

		/**
		* This method is called to setup the vertex buffer just before a call
		* to render primitives
		*/
		virtual long		PrepareForDrawing() = 0;

		/**
		* This method is called to clear the vertex buffer just after a call
		* to render primitives
		*/
		virtual long		EndDrawing() = 0;
	};
}

#endif //_RD3D_VTERTEXBUFFER_H