#include "d3d_vbofunc.h"

namespace Smt_3Drd
{
	SmtVBOFunc::SmtVBOFunc()
	{
	}

	SmtVBOFunc::~SmtVBOFunc()
	{
	}

	long SmtVBOFunc::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtVBOFunc::glGenBuffers(int count, uint *handle)
	{
		;
	}

	void SmtVBOFunc::glDeleteBuffers(int count, const uint *handle)
	{
		;
	}

	void SmtVBOFunc::glBindBuffer(uint target, uint handle)
	{
		;
	}

	void SmtVBOFunc::glBufferData(uint target, int size, void *data, uint method)
	{
		;
	}

	void* SmtVBOFunc::glMapBuffer(uint target, uint access)
	{
		return NULL;
	}

	uchar SmtVBOFunc::glUnmapBuffer(uint target)
	{
		return 0;
	}

	void SmtVBOFunc::glGetBufferParameteriv(uint target, uint param, int *value)
	{
		;
	}

}