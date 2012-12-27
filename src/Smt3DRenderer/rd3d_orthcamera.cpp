#include "rd3d_camera.h"
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtOrthCamera::SmtOrthCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport):SmtCamera(p3DRenderDevice,viewport)
		,m_bIdentity(false)
		,m_bInverse(false)
	{
		;
	}

	SmtOrthCamera::~SmtOrthCamera(void)
	{	
		m_p3DRenderDevice = NULL;
	}

	long SmtOrthCamera::Apply(void)
	{
		SmtCamera::Apply();

		m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
		m_p3DRenderDevice->MatrixLoadIdentity();
		m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
		m_p3DRenderDevice->MatrixLoadIdentity();
		
		if (!m_bIdentity&& m_bInverse)
		{
			m_p3DRenderDevice->MatrixScale(2.0f / m_viewport.ulWidth, -2.0f / m_viewport.ulHeight, 1.0f);
			m_p3DRenderDevice->MatrixTranslation(-(m_viewport.ulWidth / 2.0f), -(m_viewport.ulHeight / 2.0f), 0.0f);
		}

		if (m_bIdentity && m_bInverse)
		{
			m_p3DRenderDevice->MatrixScale(2.0, -2.0, 1.0);
			m_p3DRenderDevice->MatrixTranslation(-0.5, -0.5, 0.0);
		}

		if (!m_bIdentity && !m_bInverse)
		{
			m_p3DRenderDevice->MatrixScale(2.0f / m_viewport.ulWidth, 2.0f / m_viewport.ulHeight, 1.0f);
		}

		return SMT_ERR_NONE;
	}
}