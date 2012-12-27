#include "md3d_terrain.h"
#include "ml3d_api.h"
#include "rd3d_texturemanager.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	SmtTerrain::SmtTerrain():m_pClr(NULL)
		,m_fMinZ(0)
		,m_fMaxZ(0)
		,m_fXScale(1)
		,m_fYScale(1)
		,m_fZScale(1)
		,m_pVertexBuffer(NULL)
		,m_pIndexBuffer(NULL)
		,m_p3DSurf(NULL)
	{
	
	}

	SmtTerrain::~SmtTerrain()
	{
		Destroy();
		SMT_SAFE_DELETE_A(m_pClr);
	}

	long SmtTerrain::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		Smt3DObject::Init(vPos,matMaterial,szTexName);

		//////////////////////////////////////////////////////////////////////////
		m_nColorLevels = 3;

		m_pClr = new SmtColor[m_nColorLevels];
		m_pClr[0].fRed = 255/255.;m_pClr[0].fGreen = 0./255.;m_pClr[0].fBlue = 0./255.; m_pClr[0].fA = 1.0;		
		m_pClr[1].fRed = 0./255.;m_pClr[1].fGreen = 255./255.;m_pClr[1].fBlue = 0./255.; m_pClr[0].fA = 1.0;		
		m_pClr[2].fRed = 0./255.;m_pClr[2].fGreen = 0./255.;m_pClr[2].fBlue = 255./255.; m_pClr[0].fA = 1.0;

		//////////////////////////////////////////////////////////////////////////
		return SMT_ERR_NONE;
	}

	long SmtTerrain::SetTerrainSurf(Smt3DSurface	*pSurf)
	{
		return SetTerrainSurfDirectly((Smt3DSurface*)(pSurf->Clone()));
	}

	long SmtTerrain::SetTerrainSurfDirectly(Smt3DSurface	*pSurf) 
	{
		Destroy();

		m_p3DSurf = pSurf;

		Aabb aabb;
		m_p3DSurf->GetAabb(&aabb);

		m_aAbb.vcMax.x	= aabb.vcMax.x;
		m_aAbb.vcMax.y	= aabb.vcMax.z;
		m_aAbb.vcMax.z	= aabb.vcMax.y;

		m_aAbb.vcMin.x	= aabb.vcMin.x;
		m_aAbb.vcMin.y	= aabb.vcMin.z;
		m_aAbb.vcMin.z	= aabb.vcMin.y;

		m_aAbb.vcCenter = (m_aAbb.vcMin+m_aAbb.vcMax)/2;
		m_vCenter = m_aAbb.vcCenter;

		m_fMinZ = aabb.vcMin.z;
		m_fMaxZ = aabb.vcMax.z;

		return SMT_ERR_NONE;
	}

	long SmtTerrain::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice || NULL == m_p3DSurf)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(m_p3DSurf->GetPointCount(),
			VF_XYZ | VF_TEXCOORD | VF_NORMAL|VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < m_p3DSurf->GetPointCount();i++)
		{
			Smt3DPoint point = m_p3DSurf->GetPoint(i);
			m_pVertexBuffer->Vertex(point.GetX(),point.GetZ(),point.GetY());
		}

		m_pIndexBuffer = p3DRenderDevice->CreateIndexBuffer(m_p3DSurf->GetTriangleCount()*3);
		m_pIndexBuffer->Lock();

		for (int i = 0; i < m_p3DSurf->GetTriangleCount(); i++)
		{
			Smt3DTriangle tri = m_p3DSurf->GetTriangle(i);
			m_pIndexBuffer->Index(tri.a);
			m_pIndexBuffer->Index(tri.b);
			m_pIndexBuffer->Index(tri.c);
		}

		m_pIndexBuffer->Unlock();

		//create color
		CreateColor();

		//create texture coord
		CreateTextureCoord();

		//create terrain normal
		CreateNormal();

		m_pVertexBuffer->Unlock();

		return SMT_ERR_NONE;
	}

	long SmtTerrain::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long SmtTerrain::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
			return SMT_ERR_INVALID_PARAM;

		RenderTerrain(p3DRenderDevice);

		return SMT_ERR_NONE;
	}

	void SmtTerrain::RenderTerrain(LP3DRENDERDEVICE p3DRenderDevice)
	{
		SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
		if(pTex)
			p3DRenderDevice->SetTexture( pTex );
		else
			p3DRenderDevice->SetTexture( NULL );

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		/*p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);*/
		p3DRenderDevice->DrawPrimitives(PT_POINTLIST,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());
		p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST,m_pVertexBuffer,m_pIndexBuffer,0,m_pIndexBuffer->GetIndexCount()/3);
		/*p3DRenderDevice->MatrixPop();

		p3DRenderDevice->MatrixPop();*/

	}

	bool SmtTerrain::Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
			return false;

		Vector3 vOrg,vTar,vDir;

		/*p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);*/
		p3DRenderDevice->Transform2DTo3D(vOrg,vTar,point);
		/*p3DRenderDevice->MatrixPop();

		p3DRenderDevice->MatrixPop();*/

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
		}

		return false;
	}

	long SmtTerrain::Destroy()
	{
		SMT_SAFE_DELETE(m_pVertexBuffer);
		SMT_SAFE_DELETE(m_pIndexBuffer);
		SMT_SAFE_DELETE(m_p3DSurf);

		return SMT_ERR_NONE;
	}

	void SmtTerrain::GetColor(float h,SmtColor & clr)
	{
		float fMinZ = m_fMinZ;
		float fMaxZ = m_fMaxZ;

		if(m_nClrType == 1)
		{
			clr = m_pClr[0];
		}
		else if(m_nClrType == 2)
		{
			float	hs;

			hs = (h - fMinZ) / (fMaxZ - fMinZ);

			clr.fA = 1.0;
			clr.fRed   = (m_pClr[2].fRed   - m_pClr[0].fRed)   * hs + m_pClr[0].fRed;
			clr.fGreen = (m_pClr[2].fGreen - m_pClr[0].fGreen) * hs + m_pClr[0].fGreen;
			clr.fBlue  = (m_pClr[2].fBlue  - m_pClr[0].fBlue)  * hs + m_pClr[0].fBlue;
		}
		else if(m_nClrType == 3)
		{
			float	m_midHeight;
			float	hs;

			m_midHeight = (float)((fMaxZ + fMinZ) / 2.0);

			if(h <= m_midHeight)
			{
				hs = (h - fMinZ) / (m_midHeight - fMinZ);

				clr.fA = 1.0;
				clr.fRed   = (m_pClr[1].fRed   - m_pClr[0].fRed)   * hs + m_pClr[0].fRed;
				clr.fGreen = (m_pClr[1].fGreen - m_pClr[0].fGreen) * hs + m_pClr[0].fGreen;
				clr.fBlue  = (m_pClr[1].fBlue  - m_pClr[0].fBlue)  * hs + m_pClr[0].fBlue;
			}
			else
			{
				hs = (h - m_midHeight) / (fMaxZ - m_midHeight);

				clr.fA = 1.0;
				clr.fRed   = (m_pClr[2].fRed   - m_pClr[1].fRed)   * hs + m_pClr[1].fRed;
				clr.fGreen = (m_pClr[2].fGreen - m_pClr[1].fGreen) * hs + m_pClr[1].fGreen;
				clr.fBlue  = (m_pClr[2].fBlue  - m_pClr[1].fBlue)  * hs + m_pClr[1].fBlue;
			}
		}
	}

	void SmtTerrain::CreateColor()
	{
		if (NULL == m_p3DSurf)
			return;

		for (int i = 0; i < m_p3DSurf->GetPointCount();i++)
		{
			Smt3DPoint point = m_p3DSurf->GetPoint(i);

			SmtColor clr;
			GetColor(point.GetZ(),clr);
			m_pVertexBuffer->Diffuse(clr.fRed,clr.fGreen,clr.fBlue,.3);
		}
	}

	//
	void SmtTerrain::CreateTextureCoord(void)
	{
		if (NULL == m_p3DSurf)
			return;

		//calculator TextureCoord
		Aabb aabb;
		m_p3DSurf->GetAabb(&aabb);

		float fXDis = aabb.vcMax.x - aabb.vcMin.x,fYDis = aabb.vcMax.y - aabb.vcMin.y ;
		for (int i = 0; i < m_p3DSurf->GetPointCount();i++)
		{
			Smt3DPoint point = m_p3DSurf->GetPoint(i);
			m_pVertexBuffer->TexVertex((point.GetX()-aabb.vcMin.x)/fXDis*16,
				(point.GetY()-aabb.vcMin.y)/fYDis*16);
		}
	}

	//
	void SmtTerrain::CreateNormal()
	{
		if (NULL == m_p3DSurf)
			return;

		//new mem
		Vector4 *pNormals = new Vector4[m_p3DSurf->GetPointCount()];

		//calculator normal
		for(int i = 0; i < m_p3DSurf->GetTriangleCount();i++)
		{
			Smt3DTriangle tri = m_p3DSurf->GetTriangle(i);
			Smt3DPoint point1 = m_p3DSurf->GetPoint(tri.a);
			Smt3DPoint point2 = m_p3DSurf->GetPoint(tri.b);
			Smt3DPoint point3 = m_p3DSurf->GetPoint(tri.c);

			Vector4 V1(point1.GetX(),point1.GetY(),point1.GetZ()),
				V2(point2.GetX(),point2.GetY(),point2.GetZ()),
				V3(point3.GetX(),point3.GetY(),point3.GetZ());

			Vector4 &n1 = pNormals[tri.a];
			Vector4 &n2 = pNormals[tri.b];
			Vector4 &n3 = pNormals[tri.c];

			Vector4 nor1 = GalcTriangleNormal(V1, V2, V3);

			n1 += nor1;
			n2 += nor1;
			n3 += nor1;		
		}

		//normalize
		for (int i = 0; i < m_p3DSurf->GetPointCount();i++)
		{
			pNormals[i].Normalize();
			m_pVertexBuffer->Normal(pNormals[i].x,pNormals[i].z,pNormals[i].y);
		}

		SMT_SAFE_DELETE_A(pNormals);
	}
}