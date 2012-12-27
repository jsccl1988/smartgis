#include "gl_vbofuncimp.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtVBOFuncImpl::SmtVBOFuncImpl()
	{
	}

	SmtVBOFuncImpl::~SmtVBOFuncImpl()
	{
	}

	long SmtVBOFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		_glGenBuffers	= (PFNGLGENBUFFERSPROC)		pGLRenderDevice->GetProcAddress("glGenBuffersARB");
		_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)	pGLRenderDevice->GetProcAddress("glDeleteBuffersARB");
		_glBindBuffer	= (PFNGLBINDBUFFERPROC)		pGLRenderDevice->GetProcAddress("glBindBufferARB");
		_glBufferData	= (PFNGLBUFFERDATAPROC)		pGLRenderDevice->GetProcAddress("glBufferDataARB");
		_glMapBuffer    = (PFNGLMAPBUFFERPROC)      pGLRenderDevice->GetProcAddress("glMapBufferARB");
		_glUnmapBuffer  = (PFNGLUNMAPBUFFERPROC)    pGLRenderDevice->GetProcAddress("glUnmapBufferARB");
		_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) pGLRenderDevice->GetProcAddress("glGetBufferParameterivARB");

		if (NULL == _glGenBuffers || NULL == _glDeleteBuffers ||NULL == _glBindBuffer||NULL == _glBufferData||
			NULL == _glMapBuffer||NULL == _glUnmapBuffer||NULL == _glGetBufferParameteriv)
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	void SmtVBOFuncImpl::glGenBuffers(GLsizei count, GLuint *handle)
	{
		_glGenBuffers(count, handle);
	}

	void SmtVBOFuncImpl::glDeleteBuffers(GLsizei count, const GLuint *handle)
	{
		_glDeleteBuffers(count, handle);
	}

	void SmtVBOFuncImpl::glBindBuffer(GLenum target, GLuint handle)
	{
		_glBindBuffer(target, handle);
	}

	void SmtVBOFuncImpl::glBufferData(GLenum target, GLsizeiptr size, GLvoid *data, GLenum method)
	{
		_glBufferData(target, size, data, method);
	}

	void* SmtVBOFuncImpl::glMapBuffer(GLenum target, GLenum access)
	{
		return _glMapBuffer(target, access);
	}

	GLboolean SmtVBOFuncImpl::glUnmapBuffer(GLenum target)
	{
		return _glUnmapBuffer(target);
	}

	void SmtVBOFuncImpl::glGetBufferParameteriv(GLenum target, GLenum param, GLint *value)
	{
		_glGetBufferParameteriv(target, param, value);
	}

}