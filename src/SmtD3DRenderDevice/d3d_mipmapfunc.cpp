#include "d3d_mipmapfunc.h"

namespace Smt_3Drd
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

	void SmtMipmapFunc::glGenerateMipmap(uint target)
	{
		;
	}
}