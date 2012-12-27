/*
File:    d3d_vsyncfunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VSYNC_FUNCS_H
#define _VSYNC_FUNCS_H

#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DRenderDevice;
	typedef class SmtD3DRenderDevice *LPGLRENDERDEVICE;

	class SmtVSyncFunc
	{
	public:
		SmtVSyncFunc();
		virtual ~SmtVSyncFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual int			WaitForVSync();
		virtual void		EnableVSync();
		virtual void		DisableVSync();
	};
}

#endif //_VSYNC_FUNCS_H
