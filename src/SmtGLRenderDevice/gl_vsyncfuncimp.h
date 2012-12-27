/*
File:    gl_vsyncfuncimp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VSYNC_FUNCSIMP_H
#define _VSYNC_FUNCSIMP_H

#include "gl_vsyncfunc.h"
#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtVSyncFuncImpl : public SmtVSyncFunc
	{
	public:
		SmtVSyncFuncImpl();
		virtual ~SmtVSyncFuncImpl();

	public:
		virtual long				Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual int					WaitForVSync();
		virtual void				EnableVSync();
		virtual void				DisableVSync();

	private:
		PFNWGLSWAPINTERVALEXTPROC	_wglSwapInterval;

	};
}

#endif //_VSYNC_FUNCSIMP_H
