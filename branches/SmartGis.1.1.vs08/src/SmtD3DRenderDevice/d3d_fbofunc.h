/*
File:    d3d_fbofunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _FBO_FUNCS_H
#define _FBO_FUNCS_H

#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtD3DRenderDevice;
	typedef class SmtD3DRenderDevice *LPGLRENDERDEVICE;

	class SmtFBOFunc
	{
	public:
		SmtFBOFunc();
		virtual ~SmtFBOFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glGenFramebuffers(int count, uint *ids);
		virtual void		glDeleteFramebuffers(int count, uint *ids);
		virtual void		glBindFramebuffer(uint target, uint id);
		virtual uchar	glIsFramebuffer(uint id);
		virtual void		glGenRenderbuffers(int count, uint *ids);
		virtual void		glDeleteRenderbuffers(int count, uint *ids);
		virtual void		glBindRenderbuffer(uint target, uint id);
		virtual uchar	glIsRenderbuffer(uint id);
		virtual void		glRenderbufferStorage(uint target, uint internalFormat, int width, int height);
		virtual void		glFramebufferRenderbuffer(uint target, uint attachment, uint rbTarget, uint rbId);
		virtual int			getMaxColorAttachments();
		virtual void		glFramebufferTexture1D(uint target, uint attachment, uint texTarget, uint texId, int level);
		virtual void		glFramebufferTexture2D(uint target, uint attachment, uint texTarget, uint texId, int level);
		virtual void		glFramebufferTexture3D(uint target, uint attachment, uint texTarget, uint texId, int level, int zOffset);
		virtual uint		glCheckFramebufferStatus(uint target);
	};
}

#endif //_FBO_FUNCS_H
