#include "rd3d_camera.h"
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
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