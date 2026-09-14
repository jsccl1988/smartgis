#include "legacy/render/render3d/camera.h"
#include "legacy/render/render3d/3drenderdevice.h"

namespace render
{
	SmtCamera::SmtCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport):m_p3DRenderDevice(p3DRenderDevice)
		,m_viewport(viewport)
	{
		;
	}

	SmtCamera::~SmtCamera(void)
	{
		;
	}


	long SmtCamera::Apply(void)
	{
		return m_p3DRenderDevice->GetStateManager()->SetViewportState(m_viewport);
	}
}