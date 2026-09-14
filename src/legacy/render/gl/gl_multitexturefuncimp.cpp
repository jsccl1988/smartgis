#include "legacy/render/gl/gl_multitexturefuncimp.h"
#include "legacy/render/gl/gl_3drenderdevice.h"

namespace render
{
	SmtMultitextureFuncImpl::SmtMultitextureFuncImpl()
	{
	}

	SmtMultitextureFuncImpl::~SmtMultitextureFuncImpl()
	{
	}

	long SmtMultitextureFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		_glActiveTexture = (PFNGLACTIVETEXTUREPROC) pGLRenderDevice->GetProcAddress("glActiveTextureARB");

		if (NULL == _glActiveTexture)
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	void SmtMultitextureFuncImpl::glActiveTexture(GLenum texture)
	{
		_glActiveTexture(texture);
	}
}