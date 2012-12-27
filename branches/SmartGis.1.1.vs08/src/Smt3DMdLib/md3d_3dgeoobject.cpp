#include <math.h>
#include "md3d_3dgeoobject.h"
#include "geo_3dapi.h"
#include "smt_bas_struct.h"
#include "ml3d_api.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	Smt3DGeoObject::Smt3DGeoObject(void):m_pGeom(NULL)
		,m_pVertexBuffer(NULL)
		,m_pIndexBuffer(NULL)
	{

	}

	Smt3DGeoObject::~Smt3DGeoObject()
	{
		Destroy();
	}

	long Smt3DGeoObject::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		return Smt3DObject::Init(vPos,matMaterial,szTexName);
	}

	long Smt3DGeoObject::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice || 
			NULL == m_pGeom)
			return SMT_ERR_INVALID_PARAM;

		//.. Create VB
		switch (m_pGeom->GetGeometryType())
		{
		case GT3DPoint:
			Create3DPointVB(p3DRenderDevice,(Smt3DPoint*)m_pGeom);

			break;
		case GT3DMultiPoint:
			Create3DMultiPointVB(p3DRenderDevice,(Smt3DMultiPoint*)m_pGeom);

			break;
		case GT3DLineString:
			Create3DLineStringVB(p3DRenderDevice,(Smt3DLineString*)m_pGeom);

			break;
		case GT3DLinearRing:
			Create3DLinearRingVB(p3DRenderDevice,(Smt3DLinearRing*)m_pGeom);

			break;
		case GT3DMultiLineString:
			Create3DMultiLineStringVB(p3DRenderDevice,(Smt3DMultiLineString*)m_pGeom);

			break;
		case GT3DSurface:
			Create3DSurfaceVB(p3DRenderDevice,(Smt3DSurface*)m_pGeom);

			break;
		
		default:
			break;
		}
		
		m_pGeom->GetAabb(&m_aAbb);

		m_aAbb.vcMax+=m_vOrgPos;
		m_aAbb.vcMin+=m_vOrgPos;

		m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin)/2.;

		return SMT_ERR_NONE;
	}

	long Smt3DGeoObject::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long Smt3DGeoObject::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
		if(pTex)
			p3DRenderDevice->SetTexture( pTex );

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);

		//render
		switch (m_pGeom->GetGeometryType())
		{
		case GT3DPoint:
			Render3DPointVB(p3DRenderDevice);

			break;
		case GT3DMultiPoint:
			Render3DMultiPointVB(p3DRenderDevice);

			break;
		case GT3DLineString:
			Render3DLineStringVB(p3DRenderDevice);

			break;
		case GT3DLinearRing:
			Render3DLinearRingVB(p3DRenderDevice);

			break;
		case GT3DMultiLineString:
			Render3DMultiLineStringVB(p3DRenderDevice);

			break;
		case GT3DSurface:
			Render3DSurfaceVB(p3DRenderDevice);

			break;

		default:
			break;
		}

		p3DRenderDevice->MatrixPop();
		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	bool Smt3DGeoObject::Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
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
		}

		
		return false;
	}

	long Smt3DGeoObject::Destroy()
	{
		// Release VB memory
		SMT_SAFE_DELETE(m_pVertexBuffer);
		SMT_SAFE_DELETE(m_pGeom);
		SMT_SAFE_DELETE(m_pIndexBuffer);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void Smt3DGeoObject::SetGeometryDirectly(Smt3DGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = pGeom;
	}

	void Smt3DGeoObject::SetGeometry(Smt3DGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);

		if( pGeom != NULL )
			m_pGeom = pGeom->Clone();
		else
			m_pGeom = NULL;
	}

	Smt3DGeoObject *Smt3DGeoObject::Clone()
	{
		Smt3DGeoObject *pObj = new Smt3DGeoObject();
		if (pObj == NULL)
			return NULL;

		pObj->SetGeometry(m_pGeom);
		
		return pObj;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt3DGeoObject::Create3DPointVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DPoint *p3DPoint)
	{
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(1,
			VF_XYZ | VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		m_pVertexBuffer->Vertex(p3DPoint->GetX(),p3DPoint->GetZ(),p3DPoint->GetX());

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt3DGeoObject::Create3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DMultiPoint *p3DMultPoint)
	{
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(p3DMultPoint->GetNumGeometries(),
			VF_XYZ, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < p3DMultPoint->GetNumGeometries();i++)
		{
			Smt3DPoint *p3DPoint = (Smt3DPoint *)p3DMultPoint->GetGeometryRef(i);
			m_pVertexBuffer->Vertex(p3DPoint->GetX(),p3DPoint->GetZ(),p3DPoint->GetY());
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt3DGeoObject::Create3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DLineString *p3DLineString)
	{
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(p3DLineString->GetNumPoints(),
			VF_XYZ, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < p3DLineString->GetNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(p3DLineString->GetX(i),p3DLineString->GetZ(i),p3DLineString->GetY(i));
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt3DGeoObject::Create3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DLinearRing *p3DLinearRing)
	{
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(p3DLinearRing->GetNumPoints(),
			VF_XYZ, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < p3DLinearRing->GetNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(p3DLinearRing->GetX(i),p3DLinearRing->GetZ(i),p3DLinearRing->GetY(i));
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt3DGeoObject::Create3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DMultiLineString *p3DMultLinearRing)
	{
		return true;
	}

	bool Smt3DGeoObject::Create3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DSurface *p3DSurf)
	{
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(p3DSurf->GetPointCount(),
			VF_XYZ | VF_TEXCOORD | VF_NORMAL, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < p3DSurf->GetPointCount();i++)
		{
			Smt3DPoint point = p3DSurf->GetPoint(i);
			m_pVertexBuffer->Vertex(point.GetX(),point.GetZ(),point.GetY());
		}

		m_pIndexBuffer = p3DRenderDevice->CreateIndexBuffer(p3DSurf->GetTriangleCount()*3);
		m_pIndexBuffer->Lock();

		for (int i = 0; i < p3DSurf->GetTriangleCount(); i++)
		{
			Smt3DTriangle tri = p3DSurf->GetTriangle(i);
			m_pIndexBuffer->Index(tri.a);
			m_pIndexBuffer->Index(tri.b);
			m_pIndexBuffer->Index(tri.c);
		}

		m_pIndexBuffer->Unlock();

		//calculator TextureCoord
		Aabb aabb;
		p3DSurf->GetAabb(&aabb);

		float fXDis = aabb.vcMax.x - aabb.vcMin.x,fYDis = aabb.vcMax.y - aabb.vcMin.y ;
		for (int i = 0; i < p3DSurf->GetPointCount();i++)
		{
			Smt3DPoint point = p3DSurf->GetPoint(i);
			m_pVertexBuffer->TexVertex((point.GetX()-aabb.vcMin.x)/fXDis*16,
				(point.GetY()-aabb.vcMin.y)/fYDis*16);
		}

		//normal
		//new mem
		Vector4 *pNormals = new Vector4[p3DSurf->GetPointCount()];

		//calculator normal
		for(int i = 0; i < p3DSurf->GetTriangleCount();i++)
		{
			Smt3DTriangle tri = p3DSurf->GetTriangle(i);
			Smt3DPoint point1 = p3DSurf->GetPoint(tri.a);
			Smt3DPoint point2 = p3DSurf->GetPoint(tri.b);
			Smt3DPoint point3 = p3DSurf->GetPoint(tri.c);

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
		for (int i = 0; i < p3DSurf->GetPointCount();i++)
		{
			pNormals[i].Normalize();
			m_pVertexBuffer->Normal(pNormals[i].x,pNormals[i].z,pNormals[i].y);
		}

		SMT_SAFE_DELETE_A(pNormals);

		m_pVertexBuffer->Unlock();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt3DGeoObject::Render3DPointVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_POINTLIST,m_pVertexBuffer,0,1);

		return true;
	}

	bool Smt3DGeoObject::Render3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_POINTLIST,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt3DGeoObject::Render3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt3DGeoObject::Render3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt3DGeoObject::Render3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		return true;
	}

	bool Smt3DGeoObject::Render3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		Smt3DSurface *p3DSurf = (Smt3DSurface*)m_pGeom;
		p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST,m_pVertexBuffer,m_pIndexBuffer,0,p3DSurf->GetTriangleCount());
	
		return true;
	}
}