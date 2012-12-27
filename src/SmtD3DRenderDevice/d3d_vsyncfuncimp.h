/*
File:    d3d_vsyncfuncimp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VSYNC_FUNCSIMP_H
#define _VSYNC_FUNCSIMP_H

#include "d3d_vsyncfunc.h"
#include "d3d_prerequisites.h"

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
	};
}

#endif //_VSYNC_FUNCSIMP_H
