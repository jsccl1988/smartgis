#include "d3d_vbofuncimp.h"
#include "d3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtVBOFuncImpl::SmtVBOFuncImpl()
	{
	}

	SmtVBOFuncImpl::~SmtVBOFuncImpl()
	{
	}

	long SmtVBOFuncImpl::Initialize(LPGLRENDERDEVICE pGLRenderDevice)
	{
		return SMT_ERR_NONE;
	}

	void SmtVBOFuncImpl::glGenBuffers(int count, uint *handle)
	{
		//_glGenBuffers(count, handle);
	}

	void SmtVBOFuncImpl::glDeleteBuffers(int count, const uint *handle)
	{
		//_glDeleteBuffers(count, handle);
	}

	void SmtVBOFuncImpl::glBindBuffer(uint target, uint handle)
	{
		//_glBindBuffer(target, handle);
	}

	void SmtVBOFuncImpl::glBufferData(uint target, int size, void *data, uint method)
	{
		//_glBufferData(target, size, data, method);
	}

	void* SmtVBOFuncImpl::glMapBuffer(uint target, uint access)
	{
		return NULL;

		//return _glMapBuffer(target, access);
	}

	uchar SmtVBOFuncImpl::glUnmapBuffer(uint target)
	{
		return 0;

		//return _glUnmapBuffer(target);
	}

	void SmtVBOFuncImpl::glGetBufferParameteriv(uint target, uint param, int *value)
	{
		//_glGetBufferParameteriv(target, param, value);
	}
}