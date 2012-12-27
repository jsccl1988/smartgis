#include "d3d_shaderfunc.h"

namespace Smt_3Drd
{
	SmtShadersFunc::SmtShadersFunc()
	{
	}

	SmtShadersFunc::~SmtShadersFunc()
	{
	}

	long SmtShadersFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	uint SmtShadersFunc::glCreateShader(uint type)
	{
		return 0;
	}

	uint SmtShadersFunc::glCreateProgram()
	{
		return 0;
	}

	void SmtShadersFunc::glAttachShader(uint programId, uint shaderId)
	{
		;
	}

	void SmtShadersFunc::glLinkProgram(uint programId)
	{
		;
	}

	void SmtShadersFunc::glGetObjectParameteriv(uint programId, uint target, int *value)
	{
		;
	}

	void SmtShadersFunc::glUseProgram(uint programId)
	{
		;
	}

	void SmtShadersFunc::glShaderSource(uint shaderId, int size, const char** source, const int *param) //TODO: Fix definition
	{
		;
	}

	void SmtShadersFunc::glCompileShader(uint shaderId)
	{
		;
	}

	void SmtShadersFunc::glDetachShader(uint programId, uint shaderId)
	{
		;
	}

	void SmtShadersFunc::glDeleteObject(uint handle) // TODO: Is it really about shaders?
	{
		;
	}

	void SmtShadersFunc::glGetInfoLog(uint programId, int size, int *written, char *log)
	{
		;
	}

	int SmtShadersFunc::glGetUniformLocation(uint shaderId, const char* name)
	{
		return 0;
	}

	void SmtShadersFunc::glUniform1fv(int paramId, int size, const float *value)
	{
		;
	}

	void SmtShadersFunc::glUniform2fv(int paramId, int size, const float *value)
	{
		;
	}

	void SmtShadersFunc::glUniform3fv(int paramId, int size, const float *value)
	{
		;
	}

	void SmtShadersFunc::glUniform4fv(int paramId, int size, const float *value)
	{
		;
	}

	void SmtShadersFunc::glUniform4f(int paramId, float a, float b, float c, float d)
	{
		;
	}

	void SmtShadersFunc::glUniform1i(int paramId, int value)
	{
		;
	}

	void SmtShadersFunc::glGetUniformfv(uint shaderId, int paramId, float *values)
	{
		;
	}

	void SmtShadersFunc::glGetUniformiv(uint shaderId, int paramId, int *values)
	{
		;
	}
}