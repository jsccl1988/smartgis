/*
File:    gl_fbofunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _FBO_FUNCS_H
#define _FBO_FUNCS_H

#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLRenderDevice;
	typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

	class SmtFBOFunc
	{
	public:
		SmtFBOFunc();
		virtual ~SmtFBOFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glGenFramebuffers(GLsizei count, GLuint *ids);
		virtual void		glDeleteFramebuffers(GLsizei count, GLuint *ids);
		virtual void		glBindFramebuffer(GLenum target, GLuint id);
		virtual GLboolean	glIsFramebuffer(GLuint id);
		virtual void		glGenRenderbuffers(GLsizei count, GLuint *ids);
		virtual void		glDeleteRenderbuffers(GLsizei count, GLuint *ids);
		virtual void		glBindRenderbuffer(GLenum target, GLuint id);
		virtual GLboolean	glIsRenderbuffer(GLuint id);
		virtual void		glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height);
		virtual void		glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbTarget, GLuint rbId);
		virtual int			getMaxColorAttachments();
		virtual void		glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level);
		virtual void		glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level);
		virtual void		glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level, int zOffset);
		virtual GLenum		glCheckFramebufferStatus(GLenum target);
	};
}

#endif //_FBO_FUNCS_H
