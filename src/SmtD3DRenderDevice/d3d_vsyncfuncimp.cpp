#include "d3d_vsyncfuncimp.h"
#include "d3d_3drenderdevice.h"

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
		return SMT_ERR_NONE;
	}

	int SmtVSyncFuncImpl::WaitForVSync()
	{
		return 0;
	}

	void SmtVSyncFuncImpl::EnableVSync()
	{
		//_wglSwapInterval(1);;
	}

	void SmtVSyncFuncImpl::DisableVSync()
	{
		//_wglSwapInterval(0);
	}
}