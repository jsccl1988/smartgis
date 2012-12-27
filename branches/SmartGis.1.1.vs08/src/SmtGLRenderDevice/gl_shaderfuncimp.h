/*
File:    gl_shaderfunceximp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SHADERS_FUNCSIMP_H
#define _SHADERS_FUNCSIMP_H

#include "gl_shaderfunc.h"

namespace Smt_3Drd
{
	class SmtShadersFuncImpl : public SmtShadersFunc
	{
	public:
		SmtShadersFuncImpl();
		virtual ~SmtShadersFuncImpl();

	public:
		virtual long				Initialize(LPGLRENDERDEVICE pGLRenderDevice);

		virtual GLuint				glCreateShader(GLenum type);
		virtual GLuint				glCreateProgram();
		virtual void				glAttachShader(GLuint programId, GLuint shaderId);
		virtual void				glLinkProgram(GLuint programId);
		virtual void				glGetObjectParameteriv(GLhandleARB programId, GLenum target, GLint *value);
		virtual void				glUseProgram(GLuint programID);
		virtual void				glShaderSource(GLuint shaderId, GLsizei size, const GLchar** source, const GLint *param); //TODO: Fix definition
		virtual void				glCompileShader(GLuint shaderId);
		virtual void				glDetachShader(GLuint programId, GLuint shaderId);
		virtual void				glDeleteObject(GLhandleARB handle);
		virtual void				glGetInfoLog(GLhandleARB programId, GLsizei size, GLsizei *written, GLcharARB *log);
		virtual GLint				glGetUniformLocation(GLuint shaderId, const GLchar* name);
		virtual void				glUniform1fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void				glUniform2fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void				glUniform3fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void				glUniform4fv(GLint paramId, GLsizei size, const GLfloat *value);
		virtual void				glUniform4f(GLint paramId, float a, float b, float c, float d);
		virtual void				glUniform1i(GLint paramId, GLint value);
		virtual void				glGetUniformfv(GLuint shaderId, GLint paramId, GLfloat *values);
		virtual void				glGetUniformiv(GLuint shaderId, GLint paramId, GLint *values);

	private:
		PFNGLCREATESHADERPROC		_glCreateShader;
		PFNGLCREATEPROGRAMPROC		_glCreateProgram;
		PFNGLATTACHSHADERPROC		_glAttachShader;
		PFNGLLINKPROGRAMPROC		_glLinkProgram;
		PFNGLGETOBJECTPARAMETERIVARBPROC _glGetObjectParameteriv;
		PFNGLUSEPROGRAMPROC			_glUseProgram;
		PFNGLSHADERSOURCEARBPROC	_glShaderSource;
		PFNGLCOMPILESHADERARBPROC	_glCompileShader;
		PFNGLDETACHSHADERPROC		_glDetachShader;
		PFNGLDELETEOBJECTARBPROC	_glDeleteObject;
		PFNGLGETINFOLOGARBPROC		_glGetInfoLog;
		PFNGLGETUNIFORMLOCATIONARBPROC _glGetUniformLocation;
		PFNGLUNIFORM1FVARBPROC		_glUniform1fv;
		PFNGLUNIFORM2FVARBPROC		_glUniform2fv;
		PFNGLUNIFORM3FVARBPROC		_glUniform3fv;
		PFNGLUNIFORM4FVARBPROC		_glUniform4fv;
		PFNGLUNIFORM4FARBPROC		_glUniform4f;
		PFNGLUNIFORM1IARBPROC		_glUniform1i;
		PFNGLGETUNIFORMFVARBPROC	_glGetUniformfv;
		PFNGLGETUNIFORMIVARBPROC	_glGetUniformiv;
	};
}

#endif //_SHADERS_FUNCSIMP_H
