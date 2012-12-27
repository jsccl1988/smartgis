#include "d3d_shaderfuncimp.h"
#include "d3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtShadersFuncImpl::SmtShadersFuncImpl()
	{
	}

	SmtShadersFuncImpl::~SmtShadersFuncImpl()
	{
	}

	long SmtShadersFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	uint SmtShadersFuncImpl::glCreateShader(uint type)
	{
		return 0;

		//return _glCreateShader(type);
	}

	uint SmtShadersFuncImpl::glCreateProgram()
	{
		return 0;

		//return _glCreateProgram();
	}

	void SmtShadersFuncImpl::glAttachShader(uint programId, uint shaderId)
	{
		//_glAttachShader(programId, shaderId);
	}

	void SmtShadersFuncImpl::glLinkProgram(uint programId)
	{
		//_glLinkProgram(programId);
	}

	void SmtShadersFuncImpl::glGetObjectParameteriv(uint programId, uint target, int *value)
	{
		//_glGetObjectParameteriv(programId, target, value);
	}

	void SmtShadersFuncImpl::glUseProgram(uint programId)
	{
		//_glUseProgram(programId);
	}

	void SmtShadersFuncImpl::glShaderSource(uint shaderId, int size, const char** source, const int *param) //TODO: Fix definition
	{
		//_glShaderSource(shaderId, size, source, param);
	}

	void SmtShadersFuncImpl::glCompileShader(uint shaderId)
	{
		//_glCompileShader(shaderId);
	}

	void SmtShadersFuncImpl::glDetachShader(uint programId, uint shaderId)
	{
		//_glDetachShader(programId, shaderId);
	}

	void SmtShadersFuncImpl::glDeleteObject(uint handle) // TODO: Is it really about shaders?
	{
		//_glDeleteObject(handle);
	}

	void SmtShadersFuncImpl::glGetInfoLog(uint programId, int size, int *written, char *log)
	{
		//_glGetInfoLog(programId, size, written, log);
	}

	int SmtShadersFuncImpl::glGetUniformLocation(uint shaderId, const char* name)
	{
		return 0;

		//return _glGetUniformLocation(shaderId, name);
	}

	void SmtShadersFuncImpl::glUniform1fv(int paramId, int size, const float *value)
	{
		//_glUniform1fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform2fv(int paramId, int size, const float *value)
	{
		//_glUniform2fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform3fv(int paramId, int size, const float *value)
	{
		//_glUniform3fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform4fv(int paramId, int size, const float *value)
	{
		//_glUniform4fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform4f(int paramId, float a, float b, float c, float d)
	{
		//_glUniform4f(paramId, a, b, c, d);
	}

	void SmtShadersFuncImpl::glUniform1i(int paramId, int value)
	{
		//_glUniform1i(paramId, value);
	}

	void SmtShadersFuncImpl::glGetUniformfv(uint shaderId, int paramId, float *values)
	{
		//_glGetUniformfv(shaderId, paramId, values);
	}

	void SmtShadersFuncImpl::glGetUniformiv(uint shaderId, int paramId, int *values)
	{
		//_glGetUniformiv(shaderId, paramId, values);
	}
}