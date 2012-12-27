/*
File:    rd3d_videobuffer.h

Desc:    SmtVertexBuffer, VertexBuffer ³éÏó½Ó¿Ú

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_VIDEOBUFFER_H
#define _RD3D_VIDEOBUFFER_H

#include "rd3d_3drenderdefs.h"

/**
 * This is an abstract class that provides an interface on top of various
 * implementations of vertex buffers or similar structures.
 *
 * Inspired by the work of Tobin Schwaiger-Hastanan
 */
namespace Smt_3Drd
{
	enum VideoBufferStoreMethod
	{
		STATIC_DRAW,
		STATIC_READ,
		STATIC_COPY,
		DYNAMIC_DRAW,
		DYNAMIC_READ,
		DYNAMIC_COPY,
		STREAM_DRAW,
		STREAM_READ,
		STREAM_COPY
	};

	enum ArrayType
	{
		TEXTURE_COORD_ARRAY,    /*!< Texture coordinates array. */
		COLOR_ARRAY,            /*!< Color values array.        */
		INDEX_ARRAY,            /*!< Array of indices.          */
		NORMAL_ARRAY,           /*!< Array of normals.          */
		VERTEX_ARRAY            /*!< Vertex coordiantes array.  */
	};


	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class SMT_EXPORT_CLASS SmtVideoBuffer
	{
	public:
		SmtVideoBuffer(LP3DRENDERDEVICE p3DRenderDevice, uint handle,ArrayType type);
		virtual ~SmtVideoBuffer();

	public:
		inline uint				GetHandle() {return m_unHandle;}

		inline ArrayType		GetArrayType() {return m_arType;}

		virtual	long			Use();
		virtual long			Unuse();

		virtual long			Update(void *data, unsigned int size, VideoBufferStoreMethod method);
		
		virtual void			*Map(AccessMode access);
		virtual long			Unmap();

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
		uint					m_unHandle;

		ArrayType				m_arType;
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_VIDEOBUFFER_H