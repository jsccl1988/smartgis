/*
File:    gl_shaderfunc.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SHADERS_FUNCS_H
#define _SHADERS_FUNCS_H

#include "gl_prerequisites.h"

namespace Smt_3Drd
{
	class SmtGLRenderDevice;
	typedef class SmtGLRenderDevice *LPGLRENDERDEVICE;

	class SmtShadersFunc
	{
	public:
		SmtShadersFunc();
		virtual ~SmtShadersFunc();
		virtual long		Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual GLuint		glCreateShader(GLenum type);
		virtual GLuint		glCreateProgram();
		virtual void		glAttachShader(GLuint programId, GLuint shaderId);
		virtual void		glLinkProgram(GLuint programId);
		virtual void		glGetObjectParameteriv(GLhandleARB programId, GLenum target, GLint *value);
		virtual void		glUseProgram(GLuint programID);
		virtual void		glShaderSource(GLuint shaderId, GLsizei size, const GLchar** source, const GLint *param); //TODO: Fix definition
		virtual void		glCompileShader(GLuint shaderId);
		virtual void		glDetachShader(GLuint programId, GLuint shaderId);
		virtual void		glDeleteObject(GLhandleARB handle);
		virtual void		glGetInfoLog(GLhandleARB programId, GLsizei size, GLsizei *written, GLcharARB *log);
		virtual GLint		glGetUniformLocation(GLuint shaderId, const char* name);
		virtual void		glUniform1fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void		glUniform2fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void		glUniform3fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void		glUniform4fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void		glUniform4f(GLint paramId, float a, float b, float c, float d);
		virtual void		glUniform1i(GLint paramId, GLint value);
		virtual void		glGetUniformfv(GLuint shaderId, GLint paramId, GLfloat *values);
		virtual void		glGetUniformiv(GLuint shaderId, GLint paramId, GLint *values);
	};
}

#endif //_SHADERS_FUNCS_H
