#include "gl_fbofuncimp.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtFBOFuncImpl::SmtFBOFuncImpl()
	{
	}

	SmtFBOFuncImpl::~SmtFBOFuncImpl()
	{
	}

	long SmtFBOFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSEXTPROC) pGLRenderDevice->GetProcAddress("glGenFramebuffersEXT");
		_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSEXTPROC) pGLRenderDevice->GetProcAddress("glDeleteFramebuffersEXT");
		_glBindFramebuffer = (PFNGLBINDFRAMEBUFFEREXTPROC) pGLRenderDevice->GetProcAddress("glBindFramebufferEXT");
		_glIsFramebuffer = (PFNGLISFRAMEBUFFEREXTPROC) pGLRenderDevice->GetProcAddress("glIsFramebufferEXT");
		_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSEXTPROC) pGLRenderDevice->GetProcAddress("glGenRenderbuffersEXT");
		_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSEXTPROC) pGLRenderDevice->GetProcAddress("glDeleteRenderbuffersEXT");
		_glBindRenderbuffer = (PFNGLBINDRENDERBUFFEREXTPROC) pGLRenderDevice->GetProcAddress("glBindRenderbufferEXT");
		_glIsRenderbuffer = (PFNGLISRENDERBUFFEREXTPROC) pGLRenderDevice->GetProcAddress("glIsRenderbufferEXT");
		_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEEXTPROC) pGLRenderDevice->GetProcAddress("glRenderbufferStorageEXT");
		_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) pGLRenderDevice->GetProcAddress("glFramebufferRenderbufferEXT");
		_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) pGLRenderDevice->GetProcAddress("glFramebufferTexture1D");
		_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) pGLRenderDevice->GetProcAddress("glFramebufferTexture2D");
		_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) pGLRenderDevice->GetProcAddress("glFramebufferTexture3D");
		_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) pGLRenderDevice->GetProcAddress("glCheckFramebufferStatusEXT");
	
		if (NULL == _glGenFramebuffers || NULL == _glDeleteFramebuffers ||NULL == _glBindFramebuffer||NULL == _glIsFramebuffer||
			NULL == _glGenRenderbuffers||NULL == _glDeleteRenderbuffers||NULL == _glBindRenderbuffer||NULL == _glIsRenderbuffer||
			NULL == _glRenderbufferStorage||NULL == _glFramebufferRenderbuffer||
			/*NULL == _glFramebufferTexture1D||NULL == _glFramebufferTexture2D||NULL == _glFramebufferTexture3D||*/
			NULL == _glCheckFramebufferStatus)
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	void SmtFBOFuncImpl::glGenFramebuffers(GLsizei count, GLuint *ids)
	{
		_glGenFramebuffers(count, ids);
	}

	void SmtFBOFuncImpl::glDeleteFramebuffers(GLsizei count, GLuint *ids)
	{
		_glDeleteFramebuffers(count, ids);
	}

	void SmtFBOFuncImpl::glBindFramebuffer(GLenum target, GLuint id)
	{
		_glBindFramebuffer(target, id);
	}

	GLboolean SmtFBOFuncImpl::glIsFramebuffer(GLuint id)
	{
		return _glIsFramebuffer(id);
	}

	void SmtFBOFuncImpl::glGenRenderbuffers(GLsizei count, GLuint *ids)
	{
		_glGenRenderbuffers(count, ids);
	}

	void SmtFBOFuncImpl::glDeleteRenderbuffers(GLsizei count, GLuint *ids)
	{
		_glDeleteRenderbuffers(count, ids);
	}

	void SmtFBOFuncImpl::glBindRenderbuffer(GLenum target, GLuint id)
	{
		_glBindRenderbuffer(target, id);
	}

	GLboolean SmtFBOFuncImpl::glIsRenderbuffer(GLuint id)
	{
		return _glIsRenderbuffer(id);
	}

	void SmtFBOFuncImpl::glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
	{
		_glRenderbufferStorage(target, internalFormat, width, height);
	}

	void SmtFBOFuncImpl::glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbTarget, GLuint rbId)
	{
		_glFramebufferRenderbuffer(target, attachment, rbTarget, rbId);
	}

	int SmtFBOFuncImpl::getMaxColorAttachments()
	{
		int maxColors = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColors);
		return maxColors;
	}

	void SmtFBOFuncImpl::glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level)
	{
		_glFramebufferTexture1D(target, attachment, texTarget, texId, level);
	}

	void SmtFBOFuncImpl::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level)
	{
		_glFramebufferTexture2D(target, attachment, texTarget, texId, level);
	}

	void SmtFBOFuncImpl::glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum texTarget, GLuint texId, int level, int zOffset)
	{
		_glFramebufferTexture3D(target, attachment, texTarget, texId, level, zOffset);
	}

	GLenum SmtFBOFuncImpl::glCheckFramebufferStatus(GLenum target)
	{
		return _glCheckFramebufferStatus(target);
	}
}