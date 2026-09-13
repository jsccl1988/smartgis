#include <math.h>
#include "render/model3d/2dgeoobject.h"
#include "algorithm/geo/geometry.h"
#include "base/core/bas_struct.h"
#include "algorithm/geo/geometry.h"
#include "algorithm/tin/tin.h"

using namespace render;

namespace render
{
	Smt2DGeoObject::Smt2DGeoObject(void):m_pGeom(NULL)
		,m_pStyle(NULL)
		,m_pVertexBuffer(NULL)
		,m_pIndexBuffer(NULL)
	{

	}

	Smt2DGeoObject::~Smt2DGeoObject()
	{
		Destroy();
	}

	long Smt2DGeoObject::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		return Smt3DObject::Init(vPos,matMaterial,szTexName);
	}

	long Smt2DGeoObject::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		//.. Create VB
		switch (wkbFlatten(m_pGeom->getGeometryType()))
		{
		case wkbPoint:
			CreatePointVB(p3DRenderDevice,(OGRPoint*)m_pGeom);
			break;
		case wkbMultiPoint:
			CreateMultiPointVB(p3DRenderDevice,(OGRMultiPoint*)m_pGeom);
			break;
		case wkbLineString:
			CreateLineStringVB(p3DRenderDevice,(OGRLineString*)m_pGeom);
			break;
		case wkbLinearRing:
			CreateLinearRingVB(p3DRenderDevice,(OGRLinearRing*)m_pGeom);
			break;
		case wkbMultiLineString:
			CreateMultiLineStringVB(p3DRenderDevice,(OGRMultiLineString*)m_pGeom);
			break;
		case wkbPolygon:
			CreatePolygonVB(p3DRenderDevice,(OGRPolygon*)m_pGeom);
			break;

		default:
			break;
		}
		
		Envelope env;
		copy_envelope(*m_pGeom, &env);

		m_aAbb.merge(env.MinX,-1,env.MinY);
		m_aAbb.merge(env.MaxX,1,env.MaxY);


		m_aAbb.vcMax+=m_vOrgPos;
		m_aAbb.vcMin+=m_vOrgPos;

		m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin)/2.;

		return SMT_ERR_NONE;
	}

	long Smt2DGeoObject::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long Smt2DGeoObject::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
		if(pTex)
			p3DRenderDevice->SetTexture(pTex);

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);
		
		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);

		//render
		switch (wkbFlatten(m_pGeom->getGeometryType()))
		{
		case wkbPoint:
			RenderPointVB(p3DRenderDevice);

			break;
		case wkbMultiPoint:
			RenderMultiPointVB(p3DRenderDevice);

			break;
		case wkbLineString:
			RenderLineStringVB(p3DRenderDevice);

			break;
		case wkbLinearRing:
			RenderLinearRingVB(p3DRenderDevice);

			break;
		case wkbMultiLineString:
			RenderMultiLineStringVB(p3DRenderDevice);

			break;
		case wkbPolygon:
			RenderPolygonVB(p3DRenderDevice);

			break;

		default:
			break;
		}

		p3DRenderDevice->MatrixPop();
		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	bool Smt2DGeoObject::Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
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
				OGRPoint oPoint(vTar.x,vTar.z);
				switch (wkbFlatten(m_pGeom->getGeometryType()))
				{
					case wkbPolygon:
						{
							return m_pGeom->Contains(&oPoint);
						}
						break;
				}

				return false;
			}
		}
		
		return false;
	}

	long Smt2DGeoObject::Destroy()
	{
		// Release VB memory
		SMT_SAFE_DELETE(m_pVertexBuffer);
		SMT_SAFE_DELETE(m_pGeom);
		SMT_SAFE_DELETE(m_pIndexBuffer);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void Smt2DGeoObject::SetGeometryDirectly(OGRGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = pGeom;
	}

	void Smt2DGeoObject::SetGeometry(OGRGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);

		if( pGeom != NULL )
			m_pGeom = pGeom->clone();
		else
			m_pGeom = NULL;
	}

	void  Smt2DGeoObject::SetStyle(const SmtStyle *pStyle) 
	{ 
		if(pStyle == NULL) 
			return;

		SMT_SAFE_DELETE(m_pStyle);

		m_pStyle = pStyle->clone(pStyle->get_style_name());
	}

	Smt2DGeoObject *Smt2DGeoObject::Clone()
	{
		Smt2DGeoObject *pObj = new Smt2DGeoObject();
		if (pObj == NULL)
			return NULL;

		pObj->SetGeometry(m_pGeom);
		
		return pObj;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt2DGeoObject::CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice,OGRPoint *pPoint)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(1,
			VF_XYZ | VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		m_pVertexBuffer->Vertex(pPoint->getX(),0,pPoint->getY());
		m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);

		m_pVertexBuffer->Unlock();
		return true;
	}

	bool Smt2DGeoObject::CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,OGRMultiPoint *pMultPoint)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pMultPoint->getNumGeometries(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pMultPoint->getNumGeometries();i++)
		{
			OGRPoint *pPoint = (OGRPoint *)pMultPoint->getGeometryRef(i);

			m_pVertexBuffer->Vertex(pPoint->getX(),0,pPoint->getY());
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLineString *pLineString)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pLineString->getNumPoints(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pLineString->getNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(pLineString->getX(i),0,pLineString->getY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLineString *pSpline)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pSpline->getNumPoints(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pSpline->getNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(pSpline->getX(i),0,pSpline->getY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLinearRing *pLinearRing)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pLinearRing->getNumPoints(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pLinearRing->getNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(pLinearRing->getX(i),0,pLinearRing->getY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,OGRMultiLineString *pMultLinearRing)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();

		int nDotNum = 0;
		for(int i = 0; i < pMultLinearRing->getNumGeometries();i++)
		{
			OGRLineString *pLineString = (OGRLineString *)pMultLinearRing->getGeometryRef(i); 
			nDotNum += pLineString->getNumPoints();
		}

		//////////////////////////////////////////////////////////////////////////
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(nDotNum,
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for(int i = 0; i < pMultLinearRing->getNumGeometries();i++)
		{
			OGRLineString *pLineString = (OGRLineString *)pMultLinearRing->getGeometryRef(i); 
			for (int i = 0; i < pLineString->getNumPoints();i++)
			{
				m_pVertexBuffer->Vertex(pLineString->getX(i),0,pLineString->getY(i));
				m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
			}
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice,OGRPolygon *pPolygon)
	{
		SmtPenDesc &penDesc = m_pStyle->get_pen_desc();
		SmtBrushDesc &brushDesc = m_pStyle->get_brush_desc();

		OGRLinearRing *pLinearRing= pPolygon->getExteriorRing();
		int nPoints = pLinearRing->getNumPoints();
		if (nPoints < 0)
			return false;

		RawPoint *pRawPoints = new RawPoint[nPoints];
		for (int i = 0; i < nPoints; ++i) {
			pRawPoints[i].x = pLinearRing->getX(i);
			pRawPoints[i].y = pLinearRing->getY(i);
		}

		vector<SmtTriangle> vTriMesh;
		//////////////////////////////////////////////////////////////////////////
		/*if (SMT_ERR_NONE != Smtdivide_polygon_into_tri_mesh(vTriMesh,pRawPoints,nPoints))
		{
			SMT_SAFE_DELETE_A(pRawPoints);
			return false;
		}*/

		if (SMT_ERR_NONE != divide_polygon_into_tri_mesh(vTriMesh,pRawPoints,nPoints))
		{
			SMT_SAFE_DELETE_A(pRawPoints);
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(nPoints,
			VF_XYZ | VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < nPoints;i++)
		{
			m_pVertexBuffer->Vertex(pRawPoints[i].x,0,pRawPoints[i].y);
			m_pVertexBuffer->Diffuse(GetRValue(brushDesc.lBrushColor)/255.,GetGValue(brushDesc.lBrushColor)/255.,GetBValue(brushDesc.lBrushColor)/255.,1);
		}
	
		m_pIndexBuffer = p3DRenderDevice->CreateIndexBuffer(vTriMesh.size()*3);
		m_pIndexBuffer->Lock();

		for (int i = 0; i < vTriMesh.size(); i++)
		{
			m_pIndexBuffer->Index(vTriMesh[i].a);
			m_pIndexBuffer->Index(vTriMesh[i].b);
			m_pIndexBuffer->Index(vTriMesh[i].c);
		}

		m_pIndexBuffer->Unlock();
		
		m_pVertexBuffer->Unlock();

		SMT_SAFE_DELETE_A(pRawPoints);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool Smt2DGeoObject::RenderPointVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_POINTLIST,m_pVertexBuffer,0,1);

		return true;
	}

	bool Smt2DGeoObject::RenderMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_POINTLIST,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt2DGeoObject::RenderLineStringVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt2DGeoObject::RenderSplineVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt2DGeoObject::RenderLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt2DGeoObject::RenderMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawPrimitives(PT_LINESTRIP,m_pVertexBuffer,0,m_pVertexBuffer->GetVertexCount());

		return true;
	}

	bool Smt2DGeoObject::RenderPolygonVB(LP3DRENDERDEVICE p3DRenderDevice)
	{
		p3DRenderDevice->DrawIndexedPrimitives(PT_TRIANGLELIST,m_pVertexBuffer,m_pIndexBuffer,0,m_pIndexBuffer->GetIndexCount()/3);

		return true;
	}
}