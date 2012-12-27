#include "gl_fbofunc.h"

namespace Smt_3Drd
{
	SmtFBOFunc::SmtFBOFunc()
	{
	}

	SmtFBOFunc::~SmtFBOFunc()
	{
	}

	long SmtFBOFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtFBOFunc::glGenFramebuffers(GLsizei count, GLuint *ids)
	{
		;
	}

	void SmtFBOFunc::glDeleteFramebuffers(GLsizei count, GLuint *ids)
	{
		;
	}

	void SmtFBOFunc::glBindFramebuffer(GLenum target, GLuint id)
	{
		;
	}

	GLboolean SmtFBOFunc::glIsFramebuffer(GLuint id)
	{
		return false;
	}

	void SmtFBOFunc::glGenRenderbuffers(GLsizei count, GLuint *ids)
	{
		;
	}

	void SmtFBOFunc::glDeleteRenderbuffers(GLsizei count, GLuint *ids)
	{
		;
	}

	void SmtFBOFunc::glBindRenderbuffer(GLenum target, GLuint id)
	{
		;
	}

	GLboolean SmtFBOFunc::glIsRenderbuffer(GLuint id)
	{
		return false;
	}

	void SmtFBOFunc::glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
	{
		;
	}

	void SmtFBOFunc::glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbTarget, GLuint rbId)
	{
		;
	}

	int SmtFBOFunc::getMaxColorAttachments()
	{
		return 0;
	}

	void SmtFBOFunc::glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level)
	{
		;
	}

	void SmtFBOFunc::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level)
	{
		;
	}

	void SmtFBOFunc::glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level, int zOffset)
	{
		;
	}

	GLenum SmtFBOFunc::glCheckFramebufferStatus(GLenum target)
	{
		return 0;
	}
}