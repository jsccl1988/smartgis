#include "gl_vsyncfuncimp.h"
#include "gl_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtVSyncFuncImpl::SmtVSyncFuncImpl()
	{
	}

	SmtVSyncFuncImpl::~SmtVSyncFuncImpl()
	{
	}

	long SmtVSyncFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		_wglSwapInterval = (PFNWGLSWAPINTERVALEXTPROC) pGLRenderDevice->GetProcAddress("wglSwapIntervalEXT");

		if (NULL == _wglSwapInterval)
		{
			return SMT_ERR_FAILURE;
		}
		
		return SMT_ERR_NONE;
	}

	int SmtVSyncFuncImpl::WaitForVSync()
	{
		return 0;
	}

	void SmtVSyncFuncImpl::EnableVSync()
	{
		_wglSwapInterval(1);;
	}

	void SmtVSyncFuncImpl::DisableVSync()
	{
		_wglSwapInterval(0);
	}
}