#include "gl_multitexturefuncimp.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
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