#include "geo_3dapi.h"
#include "ml3d_api.h"

long SmtRayToSphere(const Vector4& vOrg,const Vector4 &vDir,const Vector4 &vCenter,float fRaduis)
{
	if(fRaduis < 0)
		return 1;

	Vector4 vD = vOrg - vCenter;

	float b = 2.0f * (vDir*vD);
	float c = vD*vD - fRaduis* fRaduis;

	float discriminant = (b * b) - (4.0f * c);

	if(discriminant < 0.0f)
		return false;

	discriminant = sqrt(discriminant);

	float s0 = (-b + discriminant) / 2.0f;
	float s1 = (-b - discriminant) / 2.0f;

	// if one solution is >= 0, then we intersected the sphere.
	return (s0 >= 0.0f || s1 >= 0.0f);
}

long SmtCvt3DSurfTo2DTin(const Smt3DSurface *p3DSurf,SmtTin *pTin)
{
	if (NULL == p3DSurf || NULL == pTin)
		return SMT_ERR_INVALID_PARAM;

	SmtPoint *pPointBuf = new SmtPoint[p3DSurf->GetPointCount()];
	SmtTriangle *pTriBuf = new SmtTriangle[p3DSurf->GetTriangleCount()];

	for (int i = 0; i < p3DSurf->GetPointCount();i++)
	{
		Smt3DPoint o3DPoint = p3DSurf->GetPoint(i);
		pPointBuf[i].SetX(o3DPoint.GetX());
		pPointBuf[i].SetY(o3DPoint.GetY());
	}

	for (int i = 0; i < p3DSurf->GetTriangleCount();i++)
	{
		Smt3DTriangle o3DTri = p3DSurf->GetTriangle(i);
		pTriBuf[i].a = o3DTri.a;
		pTriBuf[i].b = o3DTri.b;
		pTriBuf[i].c = o3DTri.c;
		pTriBuf[i].bDelete = o3DTri.bDelete;
	}

	pTin->AddPointCollection(pPointBuf,p3DSurf->GetPointCount());
	pTin->AddTriangleCollection(pTriBuf,p3DSurf->GetTriangleCount());

	SMT_SAFE_DELETE_A(pPointBuf);
	SMT_SAFE_DELETE_A(pTriBuf);

	return SMT_ERR_NONE;
}