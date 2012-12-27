/*
File:    d3d_mipmapfunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MIPMAP_FUNCS_H
#define _MIPMAP_FUNCS_H

#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DRenderDevice;
	typedef class SmtD3DRenderDevice *LPGLRENDERDEVICE;

	class SmtMipmapFunc
	{
	public:
		SmtMipmapFunc();
		virtual ~SmtMipmapFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glGenerateMipmap(uint target);

	};
}

#endif //_MIPMAP_FUNCS_H
