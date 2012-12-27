/*
File:    d3d_mipmapfuncimp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MIPMAP_FUNCSIMP_H
#define _MIPMAP_FUNCSIMP_H

#include "d3d_mipmapfunc.h"
#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtMipmapFuncImpl : public SmtMipmapFunc
	{
	public:
		SmtMipmapFuncImpl();
		virtual ~SmtMipmapFuncImpl();

	public:
		virtual long				Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void				glGenerateMipmap(uint target);
	};
}

#endif //_VSYNC_FUNCSIMP_H
