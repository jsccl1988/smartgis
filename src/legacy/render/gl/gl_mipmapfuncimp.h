/*
File:    gl_mipmapfuncimp.h 

Desc:    

Version: Version 1.0

Writter:  �´���

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MIPMAP_FUNCSIMP_H
#define _MIPMAP_FUNCSIMP_H

#include "legacy/render/gl/gl_mipmapfunc.h"
#include "legacy/render/gl/gl_prerequisites.h"

namespace render
{
	class SmtMipmapFuncImpl : public SmtMipmapFunc
	{
	public:
		SmtMipmapFuncImpl();
		virtual ~SmtMipmapFuncImpl();

	public:
		virtual long				Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void				glGenerateMipmap(GLenum target);

	private:
		PFNGLGENERATEMIPMAPEXTPROC _glGenerateMipmap;

	};
}

#endif //_VSYNC_FUNCSIMP_H
