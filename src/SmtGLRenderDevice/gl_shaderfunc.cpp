#include "gl_shaderfunc.h"

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

	GLuint SmtShadersFunc::glCreateShader(GLenum type)
	{
		return 0;
	}

	GLuint SmtShadersFunc::glCreateProgram()
	{
		return 0;
	}

	void SmtShadersFunc::glAttachShader(GLuint programId, GLuint shaderId)
	{
		;
	}

	void SmtShadersFunc::glLinkProgram(GLuint programId)
	{
		;
	}

	void SmtShadersFunc::glGetObjectParameteriv(GLhandleARB programId, GLenum target, GLint *value)
	{
		;
	}

	void SmtShadersFunc::glUseProgram(GLuint programId)
	{
		;
	}

	void SmtShadersFunc::glShaderSource(GLuint shaderId, GLsizei size, const GLchar** source, const GLint *param) //TODO: Fix definition
	{
		;
	}

	void SmtShadersFunc::glCompileShader(GLuint shaderId)
	{
		;
	}

	void SmtShadersFunc::glDetachShader(GLuint programId, GLuint shaderId)
	{
		;
	}

	void SmtShadersFunc::glDeleteObject(GLhandleARB handle) // TODO: Is it really about shaders?
	{
		;
	}

	void SmtShadersFunc::glGetInfoLog(GLhandleARB programId, GLsizei size, GLsizei *written, GLcharARB *log)
	{
		;
	}

	GLint SmtShadersFunc::glGetUniformLocation(GLuint shaderId, const GLchar* name)
	{
		return 0;
	}

	void SmtShadersFunc::glUniform1fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		;
	}

	void SmtShadersFunc::glUniform2fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		;
	}

	void SmtShadersFunc::glUniform3fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		;
	}

	void SmtShadersFunc::glUniform4fv(GLint paramId, GLsizei size, const GLfloat *value)
	{
		;
	}

	void SmtShadersFunc::glUniform4f(GLint paramId, float a, float b, float c, float d)
	{
		;
	}

	void SmtShadersFunc::glUniform1i(GLint paramId, GLint value)
	{
		;
	}

	void SmtShadersFunc::glGetUniformfv(GLuint shaderId, GLint paramId, GLfloat *values)
	{
		;
	}

	void SmtShadersFunc::glGetUniformiv(GLuint shaderId, GLint paramId, GLint *values)
	{
		;
	}
}