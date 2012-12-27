#include "d3d_fbofunc.h"

namespace Smt_3Drd
{
	SmtFBOFunc::SmtFBOFunc()
	{
	}

	SmtFBOFunc::~SmtFBOFunc()
	{
	}

	long SmtFBOFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtFBOFunc::glGenFramebuffers(int count, uint *ids)
	{
		;
	}

	void SmtFBOFunc::glDeleteFramebuffers(int count, uint *ids)
	{
		;
	}

	void SmtFBOFunc::glBindFramebuffer(uint target, uint id)
	{
		;
	}

	uchar SmtFBOFunc::glIsFramebuffer(uint id)
	{
		return false;
	}

	void SmtFBOFunc::glGenRenderbuffers(int count, uint *ids)
	{
		;
	}

	void SmtFBOFunc::glDeleteRenderbuffers(int count, uint *ids)
	{
		;
	}

	void SmtFBOFunc::glBindRenderbuffer(uint target, uint id)
	{
		;
	}

	uchar SmtFBOFunc::glIsRenderbuffer(uint id)
	{
		return false;
	}

	void SmtFBOFunc::glRenderbufferStorage(uint target, uint internalFormat, int width, int height)
	{
		;
	}

	void SmtFBOFunc::glFramebufferRenderbuffer(uint target, uint attachment, uint rbTarget, uint rbId)
	{
		;
	}

	int SmtFBOFunc::getMaxColorAttachments()
	{
		return 0;
	}

	void SmtFBOFunc::glFramebufferTexture1D(uint target, uint attachment, uint texTarget, uint texId, int level)
	{
		;
	}

	void SmtFBOFunc::glFramebufferTexture2D(uint target, uint attachment, uint texTarget, uint texId, int level)
	{
		;
	}

	void SmtFBOFunc::glFramebufferTexture3D(uint target, uint attachment, uint texTarget, uint texId, int level, int zOffset)
	{
		;
	}

	uint SmtFBOFunc::glCheckFramebufferStatus(uint target)
	{
		return 0;
	}
}