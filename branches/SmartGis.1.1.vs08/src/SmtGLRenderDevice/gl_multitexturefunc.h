/*
File:    gl_multitexturefunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MULTITEXTURE_FUNCS_H
#define _MULTITEXTURE_FUNCS_H

#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLRenderDevice;
	typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

	class SmtMultitextureFunc
	{
	public:
		SmtMultitextureFunc();
		virtual ~SmtMultitextureFunc();

	public:
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glActiveTexture(GLenum texture);
	};
}

#endif //_MULTITEXTURE_FUNCS_H
