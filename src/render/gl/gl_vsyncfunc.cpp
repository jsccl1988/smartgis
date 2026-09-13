#include "render/gl/gl_vsyncfunc.h"

namespace render
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