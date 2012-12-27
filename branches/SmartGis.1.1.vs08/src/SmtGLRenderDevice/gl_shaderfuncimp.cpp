#include "gl_shaderfuncimp.h"
#include "gl_3drenderdevice.h"

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
		_glCreateShader = (PFNGLCREATESHADERPROC) pGLRenderDevice->GetProcAddress("glCreateShaderObjectARB");
		_glCreateProgram = (PFNGLCREATEPROGRAMPROC) pGLRenderDevice->GetProcAddress("glCreateProgramObjectARB");
		_glAttachShader = (PFNGLATTACHSHADERPROC) pGLRenderDevice->GetProcAddress("glAttachObjectARB");
		_glLinkProgram = (PFNGLLINKPROGRAMPROC) pGLRenderDevice->GetProcAddress("glLinkProgramARB");
		_glGetObjectParameteriv = (PFNGLGETOBJECTPARAMETERIVARBPROC) pGLRenderDevice->GetProcAddress("glGetObjectParameterivARB");
		_glUseProgram = (PFNGLUSEPROGRAMPROC) pGLRenderDevice->GetProcAddress("glUseProgramObjectARB");
		_glShaderSource = (PFNGLSHADERSOURCEARBPROC) pGLRenderDevice->GetProcAddress("glShaderSourceARB");
		_glCompileShader = (PFNGLCOMPILESHADERARBPROC) pGLRenderDevice->GetProcAddress("glCompileShaderARB");
		_glDetachShader = (PFNGLDETACHSHADERPROC) pGLRenderDevice->GetProcAddress("glDetachObjectARB");
		_glDeleteObject = (PFNGLDELETEOBJECTARBPROC) pGLRenderDevice->GetProcAddress("glDeleteObjectARB");
		_glGetInfoLog = (PFNGLGETINFOLOGARBPROC) pGLRenderDevice->GetProcAddress("glGetInfoLogARB");
		_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONARBPROC) pGLRenderDevice->GetProcAddress("glGetUniformLocationARB");
		_glUniform1fv = (PFNGLUNIFORM1FVARBPROC) pGLRenderDevice->GetProcAddress("glUniform1fvARB");
		_glUniform2fv = (PFNGLUNIFORM2FVARBPROC) pGLRenderDevice->GetProcAddress("glUniform2fvARB");
		_glUniform3fv = (PFNGLUNIFORM3FVARBPROC) pGLRenderDevice->GetProcAddress("glUniform3fvARB");
		_glUniform4fv = (PFNGLUNIFORM4FVARBPROC) pGLRenderDevice->GetProcAddress("glUniform4fvARB");
		_glUniform4f = (PFNGLUNIFORM4FARBPROC) pGLRenderDevice->GetProcAddress("glUniform4fARB");
		_glUniform1i = (PFNGLUNIFORM1IARBPROC) pGLRenderDevice->GetProcAddress("glUniform1iARB");
		_glGetUniformfv = (PFNGLGETUNIFORMFVARBPROC) pGLRenderDevice->GetProcAddress("glGetUniformfvARB");
		_glGetUniformiv = (PFNGLGETUNIFORMIVARBPROC) pGLRenderDevice->GetProcAddress("glGetUniformivARB");

		if (NULL == _glCreateShader || NULL == _glCreateProgram ||NULL == _glAttachShader||NULL == _glLinkProgram||
			NULL == _glGetObjectParameteriv||NULL == _glUseProgram||NULL == _glShaderSource||NULL == _glCompileShader||
			NULL == _glDetachShader||NULL == _glDeleteObject||NULL == _glGetInfoLog||NULL == _glGetUniformLocation||
			NULL == _glUniform1fv||NULL == _glUniform2fv||NULL == _glUniform3fv||NULL == _glUniform4fv||
			NULL == _glUniform1i||NULL == _glGetUniformfv||NULL == _glGetUniformiv)
		{
			return SMT_ERR_FAILURE;
		}
		
		return SMT_ERR_NONE;
	}

	GLuint SmtShadersFuncImpl::glCreateShader(GLenum type)
	{
		return _glCreateShader(type);
	}

	GLuint SmtShadersFuncImpl::glCreateProgram()
	{
		return _glCreateProgram();
	}

	void SmtShadersFuncImpl::glAttachShader(GLuint programId, GLuint shaderId)
	{
		_glAttachShader(programId, shaderId);
	}

	void SmtShadersFuncImpl::glLinkProgram(GLuint programId)
	{
		_glLinkProgram(programId);
	}

	void SmtShadersFuncImpl::glGetObjectParameteriv(GLhandleARB programId, GLenum target, GLint *value)
	{
		_glGetObjectParameteriv(programId, target, value);
	}

	void SmtShadersFuncImpl::glUseProgram(GLuint programId)
	{
		_glUseProgram(programId);
	}

	void SmtShadersFuncImpl::glShaderSource(GLuint shaderId, GLsizei size, const GLchar** source, const GLint *param) //TODO: Fix definition
	{
		_glShaderSource(shaderId, size, source, param);
	}

	void SmtShadersFuncImpl::glCompileShader(GLuint shaderId)
	{
		_glCompileShader(shaderId);
	}

	void SmtShadersFuncImpl::glDetachShader(GLuint programId, GLuint shaderId)
	{
		_glDetachShader(programId, shaderId);
	}

	void SmtShadersFuncImpl::glDeleteObject(GLhandleARB handle) // TODO: Is it really about shaders?
	{
		_glDeleteObject(handle);
	}

	void SmtShadersFuncImpl::glGetInfoLog(GLhandleARB programId, GLsizei size, GLsizei *written, GLcharARB *log)
	{
		_glGetInfoLog(programId, size, written, log);
	}

	GLint SmtShadersFuncImpl::glGetUniformLocation(GLuint shaderId, const GLchar* name)
	{
		return _glGetUniformLocation(shaderId, name);
	}

	void SmtShadersFuncImpl::glUniform1fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		_glUniform1fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform2fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		_glUniform2fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform3fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		_glUniform3fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform4fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		_glUniform4fv(paramId, size, value);
	}

	void SmtShadersFuncImpl::glUniform4f(GLint paramId, float a, float b, float c, float d)
	{
		_glUniform4f(paramId, a, b, c, d);
	}

	void SmtShadersFuncImpl::glUniform1i(GLint paramId, GLint value)
	{
		_glUniform1i(paramId, value);
	}

	void SmtShadersFuncImpl::glGetUniformfv(GLuint shaderId, GLint paramId, GLfloat *values)
	{
		_glGetUniformfv(shaderId, paramId, values);
	}

	void SmtShadersFuncImpl::glGetUniformiv(GLuint shaderId, GLint paramId, GLint *values)
	{
		_glGetUniformiv(shaderId, paramId, values);
	}

}