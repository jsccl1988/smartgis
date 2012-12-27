/*
File:    d3d_shaderfunceximp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _SHADERS_FUNCSIMP_H
#define _SHADERS_FUNCSIMP_H

#include "d3d_shaderfunc.h"

namespace Smt_3Drd
{
	class SmtShadersFuncImpl : public SmtShadersFunc
	{
	public:
		SmtShadersFuncImpl();
		virtual ~SmtShadersFuncImpl();

	public:
		virtual long				Initialize(LPGLRENDERDEVICE pGLRenderDevice);

		virtual uint				glCreateShader(uint type);
		virtual uint				glCreateProgram();
		virtual void				glAttachShader(uint programId, uint shaderId);
		virtual void				glLinkProgram(uint programId);
		virtual void				glGetObjectParameteriv(uint programId, uint target, int *value);
		virtual void				glUseProgram(uint programID);
		virtual void				glShaderSource(uint shaderId, int size, const char** source, const int *param); //TODO: Fix definition
		virtual void				glCompileShader(uint shaderId);
		virtual void				glDetachShader(uint programId, uint shaderId);
		virtual void				glDeleteObject(uint handle);
		virtual void				glGetInfoLog(uint programId, int size, int *written, char *log);
		virtual int					glGetUniformLocation(uint shaderId, const char* name);
		virtual void				glUniform1fv(int paramId, int size, const float *value);
		virtual void				glUniform2fv(int paramId, int size, const float *value);
		virtual void				glUniform3fv(int paramId, int size, const float *value);
		virtual void				glUniform4fv(int paramId, int size, const float *value);
		virtual void				glUniform4f(int paramId, float a, float b, float c, float d);
		virtual void				glUniform1i(int paramId, int value);
		virtual void				glGetUniformfv(uint shaderId, int paramId, float *values);
		virtual void				glGetUniformiv(uint shaderId, int paramId, int *values);
	};
}

#endif //_SHADERS_FUNCSIMP_H
