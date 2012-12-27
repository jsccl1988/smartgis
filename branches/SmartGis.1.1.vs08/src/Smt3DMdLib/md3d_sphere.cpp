#include <math.h>
#include "md3d_sphere.h"
#include "geo_3dapi.h"
#include "smt_bas_struct.h "

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	SmtSphere::SmtSphere(float radius, DWORD slices ):m_dwSlices(slices)
		,m_fRadius(radius)
		,m_pVertexBuffer(NULL)
		,m_fXScale(1)
		,m_fYScale(1)
		,m_fZScale(1)
	{

	}

	SmtSphere::~SmtSphere()
	{
		Destroy();
	}

	long SmtSphere::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		return Smt3DObject::Init(vPos,matMaterial,szTexName);
	}

	long SmtSphere::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		// Create VB
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
			2*(m_dwSlices+1)*m_dwSlices,
			VF_XYZ | VF_TEXCOORD | VF_NORMAL, 
			false );

		DWORD i=0, j=0;
		float Phi=0, Theta=0;
		float PhiInc   = -PI/m_dwSlices;
		float ThetaInc = -2*PI/m_dwSlices;
		Vector3 vPos;
		Vector3 vNorm; 

		if (SMT_ERR_NONE == m_pVertexBuffer->Lock())
		{
			for ( i=0; i< m_dwSlices; i++ )
			{
				Theta = 0;

				for ( j=0; j<=m_dwSlices; j++ )
				{			
					// Position and normal computation for 1st point of strip
					//--
					vPos.x = m_fRadius*cosf(Theta)*sinf(Phi);
					vPos.y = m_fRadius*sinf(Theta)*sinf(Phi);
					vPos.z = m_fRadius*cosf(Phi);
					
					vPos += m_vOrgPos;
					vPos.x *=m_fXScale;
					vPos.y *=m_fYScale;
					vPos.z *=m_fZScale;

					vNorm  = vPos;
					vNorm.Normalize();
					vNorm.Negate();

					m_pVertexBuffer->Normal( vNorm.x, vNorm.z, vNorm.y  );
					m_pVertexBuffer->TexVertex( Theta/(2*PI), Phi/PI );
					m_pVertexBuffer->Vertex( vPos.x,vPos.z,vPos.y);

					// Position and normal computation for 2nd point of strip
					//--
					vPos.x = m_fRadius*cosf(Theta)*sinf(Phi+PhiInc);
					vPos.y = m_fRadius*sinf(Theta)*sinf(Phi+PhiInc);
					vPos.z = m_fRadius*cosf(Phi+PhiInc);

					vPos += m_vOrgPos;
					vPos.x *=m_fXScale;
					vPos.y *=m_fYScale;
					vPos.z *=m_fZScale;

					vNorm  = vPos;
					vNorm.Normalize();
					vNorm.Negate();

					m_pVertexBuffer->Normal(vNorm.x,vNorm.z,vNorm.y);
					m_pVertexBuffer->TexVertex(Theta/(2*PI),(Phi+PhiInc)/PI);
					m_pVertexBuffer->Vertex( vPos.x,vPos.z,vPos.y);

					Theta += ThetaInc;
				}

				Phi += PhiInc;
			}

			m_pVertexBuffer->Unlock();
		}
	
		m_aAbb.vcMax.Set(m_fRadius*m_fXScale,m_fRadius*m_fYScale,m_fRadius*m_fZScale);
		m_aAbb.vcMin.Set(-m_fRadius*m_fXScale,-m_fRadius*m_fYScale,-m_fRadius*m_fZScale);

		m_aAbb.vcMax+=m_vOrgPos;
		m_aAbb.vcMin+=m_vOrgPos;

		m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin)/2.;

		return SMT_ERR_NONE;
	}

	long SmtSphere::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long SmtSphere::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
		if(pTex)
		{
			p3DRenderDevice->SetTexture( pTex );
		}

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);
		
		DWORD i=0;
		for ( i=0; i<m_dwSlices; i++ )
		{
			p3DRenderDevice->DrawPrimitives(
				PT_TRIANGLESTRIP,
				m_pVertexBuffer,
				i*2*(m_dwSlices+1),
				2*m_dwSlices );
		}

		p3DRenderDevice->MatrixPop();
		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	bool SmtSphere::Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
			return false;
	
		Vector3 vOrg,vTar,vDir;

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);
		p3DRenderDevice->Transform2DTo3D(vOrg,vTar,point);
		p3DRenderDevice->MatrixPop();

		p3DRenderDevice->MatrixPop();

		vDir = vTar-vOrg;
		if (vDir.GetSqrLength() > 0)
		{
			Ray		ray;
			float   f;
			ray.Set(vOrg,vDir);
			if (m_aAbb.Intersects(ray,&f))
			{
				return true;
			}

			/*Vector3 vSqrDis(vTar-m_vOrgPos);
			if (vSqrDis.GetSqrLength() < m_fRadius*m_fRadius)
			{
				return true;
			}*/
		}
		
		return false;
	}

	long SmtSphere::Destroy()
	{
		// Release VB memory
		SMT_SAFE_DELETE( m_pVertexBuffer);

		return SMT_ERR_NONE;
	}
}