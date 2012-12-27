#include "d3d_mipmapfuncimp.h"
#include "d3d_3drenderdevice.h"

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
		return SMT_ERR_NONE;
	}

	void SmtMipmapFuncImpl::glGenerateMipmap(uint target)
	{
		 //_glGenerateMipmap(target);
	}
}