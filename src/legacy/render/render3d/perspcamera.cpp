#include "legacy/render/render3d/camera.h"
#include "legacy/render/render3d/3drenderdevice.h"

namespace render
{
	SmtPerspCamera::SmtPerspCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport):SmtCamera(p3DRenderDevice,viewport)
		,m_fSmoothX(0)
		,m_fMoveStep(5.)
	{
		m_vEye		= Vector3(0.,0.,0.);					
		m_vTarget	= Vector3(0.0,1.0, 0.5);				
		m_vUp		= Vector3(0.,0.,1.);	
	}

	SmtPerspCamera::~SmtPerspCamera(void)
	{	
		m_p3DRenderDevice = NULL;
	}

	//move camera
	void SmtPerspCamera::MoveForward(void)
	{
		Vector3 vDir=m_vTarget-m_vEye;
		vDir.normalize();

		m_vEye.x	+=vDir.x*m_fMoveStep;
		m_vEye.z	+=vDir.z*m_fMoveStep;
		m_vTarget.x	+=vDir.x*m_fMoveStep;
		m_vTarget.z	+=vDir.z*m_fMoveStep;
	}

	void SmtPerspCamera::MoveBack(void)
	{
		Vector3 vDir=m_vTarget-m_vEye;
		vDir.normalize();

		m_vEye.x	-=vDir.x*m_fMoveStep;
		m_vEye.z	-=vDir.z*m_fMoveStep;
		m_vTarget.x	-=vDir.x*m_fMoveStep;
		m_vTarget.z	-=vDir.z*m_fMoveStep;
	}

	void SmtPerspCamera::MoveLeft(void)
	{
		Vector3 vCross,vDir(m_vTarget - m_vEye);
		vCross = vDir.cross(m_vUp);
		vCross.normalize();

		m_vEye.x -= vCross.x * m_fMoveStep;
		m_vEye.z -= vCross.z * m_fMoveStep;

		m_vTarget.x -= vCross.x * m_fMoveStep;
		m_vTarget.z -= vCross.z * m_fMoveStep;
	}

	void SmtPerspCamera::MoveRight(void)
	{
		Vector3 vCross,vDir(m_vTarget - m_vEye);
		vCross = vDir.cross(m_vUp);
		vCross.normalize();

		m_vEye.x += vCross.x * m_fMoveStep;
		m_vEye.z += vCross.z * m_fMoveStep;

		m_vTarget.x += vCross.x * m_fMoveStep;
		m_vTarget.z += vCross.z * m_fMoveStep;
	}

	void SmtPerspCamera::MoveUp(void)
	{
		m_vEye.y	+= m_vUp.y * m_fMoveStep;
		m_vTarget.y += m_vUp.y * m_fMoveStep;
	}

	void SmtPerspCamera::MoveDown(void)
	{
		m_vEye.y	-= m_vUp.y * m_fMoveStep;
		m_vTarget.y -= m_vUp.y * m_fMoveStep;
	}

	void SmtPerspCamera::MoveEyeSmoothly(bool bForward)
	{
		if (bForward)
			m_fSmoothX += 5.;
		else
			m_fSmoothX -= 5.;

		double angle = 0.5*atan(0.1*m_fSmoothX*20)+0.25*PI;
		float radius = tan(angle)+30*sqrt(3.0)+0.1;

		Vector3 vDir(m_vEye-m_vTarget);
		vDir.normalize();
		m_vEye = vDir*radius+m_vTarget;
	}

	void SmtPerspCamera::MoveEyeImmediately(float fDis)
	{
		Vector3 vDir(m_vEye-m_vTarget);
		vDir.normalize();
		m_vEye = vDir*fDis+m_vTarget;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtPerspCamera::Pitch(float angle)				//��x��
	{
		Vector3 vCross,vDir(m_vTarget - m_vEye);
		vCross = vDir.cross(m_vUp);
		vCross.normalize();
		vDir.rotate(vCross,angle);
		m_vTarget = m_vEye + vDir;
	}

	void SmtPerspCamera::Yaw(float angle)				//��y��
	{
		Vector3 vDir(m_vTarget - m_vEye);
		vDir.rotate(m_vUp,angle);
		m_vTarget = m_vEye + vDir;
	}

	void SmtPerspCamera::Roll(float angle)				//��z��
	{
		Vector3 vDir=m_vTarget-m_vEye;
		vDir.normalize();
		m_vUp.rotate(vDir,angle);	
	}

	long SmtPerspCamera::Apply(void)
	{
		if (m_viewport.ulWidth == 0 || m_viewport.ulHeight == 0)
		{
			return SMT_ERR_FAILURE;
		}
		if (m_viewport.fZNear <= 0.f)
		{
			m_viewport.fZNear = 0.1f;
		}
		if (m_viewport.fZFar <= m_viewport.fZNear)
		{
			m_viewport.fZFar = 1000.f;
		}
		if (m_viewport.fFovy <= 0.f)
		{
			m_viewport.fFovy = 45.f;
		}
		SmtCamera::Apply();
		m_p3DRenderDevice->MatrixModeSet(MM_PROJECTION);
		m_p3DRenderDevice->MatrixLoadIdentity();
		m_p3DRenderDevice->SetPerspective(m_viewport.fFovy, ((float)m_viewport.ulWidth) / ((float)m_viewport.ulHeight),m_viewport.fZNear,m_viewport.fZFar);
		m_p3DRenderDevice->MatrixModeSet(MM_MODELVIEW);
		m_p3DRenderDevice->MatrixLoadIdentity();
		m_p3DRenderDevice->SetViewLookAt(m_vEye,m_vTarget,m_vUp);

		return SMT_ERR_NONE;
	}
}