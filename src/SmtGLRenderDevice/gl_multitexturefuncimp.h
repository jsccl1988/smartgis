/*
File:    gl_multitexturefuncimp.h

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _MULTITEXTURE_FUNCSIMP_H
#define _MULTITEXTURE_FUNCSIMP_H

#include "gl_multitexturefunc.h"
#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtMultitextureFuncImpl : public SmtMultitextureFunc
	{
	public:
		SmtMultitextureFuncImpl();
		virtual ~SmtMultitextureFuncImpl();

	public:
		virtual long			Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void			glActiveTexture(GLenum texture);

	private:
		PFNGLACTIVETEXTUREPROC	_glActiveTexture;
	};
}

#endif //_MULTITEXTURE_FUNCSIMP_H
