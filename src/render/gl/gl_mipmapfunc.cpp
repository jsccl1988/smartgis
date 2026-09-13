#include "render/gl/gl_mipmapfunc.h"

namespace render
{
	SmtMipmapFunc::SmtMipmapFunc()
	{
	}

	SmtMipmapFunc::~SmtMipmapFunc()
	{
	}

	long SmtMipmapFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtMipmapFunc::glGenerateMipmap(GLenum target)
	{
		;
	}
}