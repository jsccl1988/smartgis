#include "gl_vbofunc.h"

namespace Smt_3Drd
{
	SmtVBOFunc::SmtVBOFunc()
	{
	}

	SmtVBOFunc::~SmtVBOFunc()
	{
	}

	long SmtVBOFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtVBOFunc::glGenBuffers(GLsizei count, GLuint *handle)
	{
		;
	}

	void SmtVBOFunc::glDeleteBuffers(GLsizei count, const GLuint *handle)
	{
		;
	}

	void SmtVBOFunc::glBindBuffer(GLenum target, GLuint handle)
	{
		;
	}

	void SmtVBOFunc::glBufferData(GLenum target, GLsizeiptr size, GLvoid *data, GLenum method)
	{
		;
	}

	void* SmtVBOFunc::glMapBuffer(GLenum target, GLenum access)
	{
		return NULL;
	}

	GLboolean SmtVBOFunc::glUnmapBuffer(GLenum target)
	{
		return GL_FALSE;
	}

	void SmtVBOFunc::glGetBufferParameteriv(GLenum target, GLenum param, GLint *value)
	{
		;
	}

}