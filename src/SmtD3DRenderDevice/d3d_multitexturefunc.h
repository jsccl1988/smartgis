/*
File:    d3d_multitexturefunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MULTITEXTURE_FUNCS_H
#define _MULTITEXTURE_FUNCS_H

#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DRenderDevice;
	typedef class SmtD3DRenderDevice *LPGLRENDERDEVICE;

	class SmtMultitextureFunc
	{
	public:
		SmtMultitextureFunc();
		virtual ~SmtMultitextureFunc();

	public:
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glActiveTexture(uint texture);
	};
}

#endif //_MULTITEXTURE_FUNCS_H
