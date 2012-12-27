#include "d3d_vsyncfunc.h"

namespace Smt_3Drd
{
	SmtVSyncFunc::SmtVSyncFunc()
	{
	}

	SmtVSyncFunc::~SmtVSyncFunc()
	{
	}

	long SmtVSyncFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	int SmtVSyncFunc::WaitForVSync()
	{
		return 0;
	}

	void SmtVSyncFunc::EnableVSync()
	{
		;
	}

	void SmtVSyncFunc::DisableVSync()
	{
		;
	}
}