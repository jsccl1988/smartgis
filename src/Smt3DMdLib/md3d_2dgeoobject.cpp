#include <math.h>
#include "md3d_2dgeoobject.h"
#include "geo_api.h"
#include "geo_3dapi.h"
#include "smt_bas_struct.h "
#include "tin_api.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
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
		switch (m_pGeom->GetGeometryType())
		{
		case GTPoint:
			CreatePointVB(p3DRenderDevice,(SmtPoint*)m_pGeom);
			break;
		case GTMultiPoint:
			CreateMultiPointVB(p3DRenderDevice,(SmtMultiPoint*)m_pGeom);
			break;
		case GTLineString:
			CreateLineStringVB(p3DRenderDevice,(SmtLineString*)m_pGeom);
			break;
		case GTSpline:
			CreateSplineVB(p3DRenderDevice,(SmtSpline*)m_pGeom);
			break;
		case GTLinearRing:
			CreateLinearRingVB(p3DRenderDevice,(SmtLinearRing*)m_pGeom);
			break;
		case GTMultiLineString:
			CreateMultiLineStringVB(p3DRenderDevice,(SmtMultiLineString*)m_pGeom);
			break;
		case GTPolygon:
			CreatePolygonVB(p3DRenderDevice,(SmtPolygon*)m_pGeom);
			break;

		default:
			break;
		}
		
		Envelope env;
		m_pGeom->GetEnvelope(&env);

		m_aAbb.Merge(env.MinX,-1,env.MinY);
		m_aAbb.Merge(env.MaxX,1,env.MaxY);


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
		switch (m_pGeom->GetGeometryType())
		{
		case GTPoint:
			RenderPointVB(p3DRenderDevice);

			break;
		case GTMultiPoint:
			RenderMultiPointVB(p3DRenderDevice);

			break;
		case GTLineString:
			RenderLineStringVB(p3DRenderDevice);

			break;
		case GTSpline:
			RenderSplineVB(p3DRenderDevice);

			break;
		case GTLinearRing:
			RenderLinearRingVB(p3DRenderDevice);

			break;
		case GTMultiLineString:
			RenderMultiLineStringVB(p3DRenderDevice);

			break;
		case GTPolygon:
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
				SmtPoint oPoint(vTar.x,vTar.z);
				switch (m_pGeom->GetGeometryType())
				{
					case GTPolygon:
						{
							return (m_pGeom->Relationship(&oPoint) == SS_Contains);
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
	void Smt2DGeoObject::SetGeometryDirectly(SmtGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = pGeom;
	}

	void Smt2DGeoObject::SetGeometry(SmtGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);

		if( pGeom != NULL )
			m_pGeom = pGeom->Clone();
		else
			m_pGeom = NULL;
	}

	void  Smt2DGeoObject::SetStyle(const SmtStyle *pStyle) 
	{ 
		if(pStyle == NULL) 
			return;

		SMT_SAFE_DELETE(m_pStyle);

		m_pStyle = pStyle->Clone(pStyle->GetStyleName());
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
	bool Smt2DGeoObject::CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice,SmtPoint *pPoint)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(1,
			VF_XYZ | VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		m_pVertexBuffer->Vertex(pPoint->GetX(),0,pPoint->GetY());
		m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);

		m_pVertexBuffer->Unlock();
		return true;
	}

	bool Smt2DGeoObject::CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,SmtMultiPoint *pMultPoint)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pMultPoint->GetNumGeometries(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pMultPoint->GetNumGeometries();i++)
		{
			SmtPoint *pPoint = (SmtPoint *)pMultPoint->GetGeometryRef(i);

			m_pVertexBuffer->Vertex(pPoint->GetX(),0,pPoint->GetY());
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,SmtLineString *pLineString)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pLineString->GetNumPoints(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pLineString->GetNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(pLineString->GetX(i),0,pLineString->GetY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice,SmtSpline *pSpline)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pSpline->GetAnalyticPointCount(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pSpline->GetAnalyticPointCount();i++)
		{
			m_pVertexBuffer->Vertex(pSpline->GetAnalyticX(i),0,pSpline->GetAnalyticY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,SmtLinearRing *pLinearRing)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(pLinearRing->GetNumPoints(),
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0; i < pLinearRing->GetNumPoints();i++)
		{
			m_pVertexBuffer->Vertex(pLinearRing->GetX(i),0,pLinearRing->GetY(i));
			m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreateMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,SmtMultiLineString *pMultLinearRing)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();

		int nDotNum = 0;
		for(int i = 0; i < pMultLinearRing->GetNumGeometries();i++)
		{
			SmtLineString *pLineString = (SmtLineString *)pMultLinearRing->GetGeometryRef(i); 
			switch (pLineString->GetGeometryType())
			{
				case GTLineString:
					nDotNum += pLineString->GetNumPoints(); 
					break;
				case GTSpline:
					{
						SmtSpline *pSpline = (SmtSpline *)pLineString;
						nDotNum += pSpline->GetAnalyticPointCount();
					}
					break;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(nDotNum,
			VF_XYZ| VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for(int i = 0; i < pMultLinearRing->GetNumGeometries();i++)
		{
			SmtLineString *pLineString = (SmtLineString *)pMultLinearRing->GetGeometryRef(i); 
			switch (pLineString->GetGeometryType())
			{
			case GTLineString:
				{
					for (int i = 0; i < pLineString->GetNumPoints();i++)
					{
						m_pVertexBuffer->Vertex(pLineString->GetX(i),0,pLineString->GetY(i));
						m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
					}
				}
				break;
			case GTSpline:
				{
					SmtSpline *pSpline = (SmtSpline *)pLineString;
					for (int i = 0; i < pSpline->GetAnalyticPointCount();i++)
					{
						m_pVertexBuffer->Vertex(pSpline->GetAnalyticX(i),0,pSpline->GetAnalyticY(i));
						m_pVertexBuffer->Diffuse(GetRValue(penDesc.lPenColor)/255.,GetGValue(penDesc.lPenColor)/255.,GetBValue(penDesc.lPenColor)/255.,1);
					}
				}
				break;
			}
		}

		m_pVertexBuffer->Unlock();

		return true;
	}

	bool Smt2DGeoObject::CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice,SmtPolygon *pPolygon)
	{
		SmtPenDesc &penDesc = m_pStyle->GetPenDesc();
		SmtBrushDesc &brushDesc = m_pStyle->GetBrushDesc();

		SmtLinearRing *pLinearRing= pPolygon->GetExteriorRing();
		int nPoints = pLinearRing->GetNumPoints();
		if (nPoints < 0)
			return false;

		RawPoint *pRawPoints = new RawPoint[nPoints];
		pLinearRing->GetPoints(pRawPoints);

		vector<SmtTriangle> vTriMesh;
		//////////////////////////////////////////////////////////////////////////
		/*if (SMT_ERR_NONE != SmtDivPolygenIntoTriMesh(vTriMesh,pRawPoints,nPoints))
		{
			SMT_SAFE_DELETE_A(pRawPoints);
			return false;
		}*/

		if (SMT_ERR_NONE != DivPolygenIntoTriMesh(vTriMesh,pRawPoints,nPoints))
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