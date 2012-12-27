/*
File:    gl_vbofunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VBO_FUNCS_H
#define _VBO_FUNCS_H

#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLRenderDevice;
	typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

	class SmtVBOFunc
	{
	public:
		SmtVBOFunc();
		virtual ~SmtVBOFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void		glGenBuffers(GLsizei count, GLuint *handle);
		virtual void		glDeleteBuffers(GLsizei count, const GLuint *handle);
		virtual void		glBindBuffer(GLenum target, GLuint handle);
		virtual void		glBufferData(GLenum target, GLsizeiptr size, GLvoid *data, GLenum method);
		virtual void*		glMapBuffer(GLenum target, GLenum access);
		virtual GLboolean	glUnmapBuffer(GLenum target);
		virtual void		glGetBufferParameteriv(GLenum target, GLenum param, GLint *value);
	};
}

#endif //_VBO_FUNCS_H
