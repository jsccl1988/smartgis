#include "d3d_fbofuncimp.h"
#include "d3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtFBOFuncImpl::SmtFBOFuncImpl()
	{
	}

	SmtFBOFuncImpl::~SmtFBOFuncImpl()
	{
	}

	long SmtFBOFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtFBOFuncImpl::glGenFramebuffers(int count, uint *ids)
	{
		//_glGenFramebuffers(count, ids);
	}

	void SmtFBOFuncImpl::glDeleteFramebuffers(int count, uint *ids)
	{
		//_glDeleteFramebuffers(count, ids);
	}

	void SmtFBOFuncImpl::glBindFramebuffer(uint target, uint id)
	{
		//_glBindFramebuffer(target, id);
	}

	uchar SmtFBOFuncImpl::glIsFramebuffer(uint id)
	{
		return 0;

		//return _glIsFramebuffer(id);
	}

	void SmtFBOFuncImpl::glGenRenderbuffers(int count, uint *ids)
	{
		//_glGenRenderbuffers(count, ids);
	}

	void SmtFBOFuncImpl::glDeleteRenderbuffers(int count, uint *ids)
	{
		//_glDeleteRenderbuffers(count, ids);
	}

	void SmtFBOFuncImpl::glBindRenderbuffer(uint target, uint id)
	{
		//_glBindRenderbuffer(target, id);
	}

	uchar SmtFBOFuncImpl::glIsRenderbuffer(uint id)
	{
		return 0;

		//return _glIsRenderbuffer(id);
	}

	void SmtFBOFuncImpl::glRenderbufferStorage(uint target, uint internalFormat, int width, int height)
	{
		//_glRenderbufferStorage(target, internalFormat, width, height);
	}

	void SmtFBOFuncImpl::glFramebufferRenderbuffer(uint target, uint attachment, uint rbTarget, uint rbId)
	{
		//_glFramebufferRenderbuffer(target, attachment, rbTarget, rbId);
	}

	int SmtFBOFuncImpl::getMaxColorAttachments()
	{
		int maxColors = 0;
		//glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxColors);

		return maxColors;
	}

	void SmtFBOFuncImpl::glFramebufferTexture1D(uint target, uint attachment, uint texTarget, uint texId, int level)
	{
		//_glFramebufferTexture1D(target, attachment, texTarget, texId, level);
	}

	void SmtFBOFuncImpl::glFramebufferTexture2D(uint target, uint attachment, uint texTarget, uint texId, int level)
	{
		//_glFramebufferTexture2D(target, attachment, texTarget, texId, level);
	}

	void SmtFBOFuncImpl::glFramebufferTexture3D(uint target, uint attachment, uint texTarget, uint texId, int level, int zOffset)
	{
		//_glFramebufferTexture3D(target, attachment, texTarget, texId, level, zOffset);
	}

	uint SmtFBOFuncImpl::glCheckFramebufferStatus(uint target)
	{
		return 0;

		//return _glCheckFramebufferStatus(target);
	}
}