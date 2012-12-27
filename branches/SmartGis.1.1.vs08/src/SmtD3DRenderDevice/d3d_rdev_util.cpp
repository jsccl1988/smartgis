#include "d3d_3drenderdevice.h"

namespace Smt_3Drd
{
	bool SmtD3DRenderDevice::IsExtensionSupported(string extension)
	{
		return false;
	}

	void* SmtD3DRenderDevice::GetProcAddress(string name)
	{
		return NULL;
	}

	SmtGPUStateManager* SmtD3DRenderDevice::GetStateManager()
	{
		return m_pStateManager;
	}

	Smt3DDeviceCaps* SmtD3DRenderDevice::GetDeviceCaps()
	{
		return m_pDeviceCaps;
	}
}