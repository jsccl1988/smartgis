#include <math.h>
#include "md3d_cube.h"

namespace Smt_3DModel
{
	SmtCube::SmtCube( LP3DRENDERDEVICE pRenderDevice, Vector3 m_vCenter, float dbfWidth )
	{
		m_vCenter = m_vCenter;
		m_fWidth = dbfWidth;
	}

	SmtCube::~SmtCube()
	{
		Destroy();
	}

	long SmtCube::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		Smt3DObject::Init(vPos,matMaterial,szTexName);

		//ÉèÖÃ²ÄÖÊ
		m_matMaterial.SetAmbientValue(SmtColor(0,0,0));
		m_matMaterial.SetDiffuseValue(SmtColor(.1,.3,1));
		m_matMaterial.SetSpecularValue(SmtColor(1,1,1));
		m_matMaterial.SetEmissiveValue(SmtColor(0.1,1,1,1));
		m_matMaterial.SetShininessValue(22);

		return SMT_ERR_NONE;
	}

	long SmtCube::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		m_aAbb.vcMax = m_vCenter+(m_fWidth/2.);
		m_aAbb.vcMin = m_vCenter-(m_fWidth/2.);
		m_aAbb.vcCenter = m_vCenter;

		return SMT_ERR_NONE;
	}

	long SmtCube::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long SmtCube::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);
		p3DRenderDevice->DrawCube3D(m_vCenter,m_fWidth,SmtColor(1.,0.,0.,1.));
		p3DRenderDevice->MatrixPop();

		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	long SmtCube::Destroy()
	{
		// Release VB memory
		SMT_SAFE_DELETE(m_pVertexBuffer);

		return SMT_ERR_NONE;
	}
}