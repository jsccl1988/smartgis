#include "gl_mipmapfuncimp.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtMipmapFuncImpl::SmtMipmapFuncImpl()
	{
	}

	SmtMipmapFuncImpl::~SmtMipmapFuncImpl()
	{
	}

	long SmtMipmapFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		_glGenerateMipmap = (PFNGLGENERATEMIPMAPEXTPROC) pGLRenderDevice->GetProcAddress("glGenerateMipmap");

		if (NULL == _glGenerateMipmap)
		{
			return SMT_ERR_FAILURE;
		}
		
		return SMT_ERR_NONE;
	}

	void SmtMipmapFuncImpl::glGenerateMipmap(GLenum target)
	{
		 _glGenerateMipmap(target);
	}
}