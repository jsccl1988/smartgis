#include "tin_api.h"
#include "geo_api.h"
#include "tin_delaunay_divide.h"
#include "tin_delaunay_incremental.h"

using namespace Smt_TinMesh;

long CreateDelaunayTin_Div(Smt3DSurface *p3DSurf,const Vector3 *pVector3Ds,int nCount)
{
	SmtTinDelaunayDiv tinDela;
	tinDela.SetVertexs(pVector3Ds,nCount);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo3DSurf(p3DSurf))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

long CreateDelaunayTin_Div(Smt3DSurface *p3DSurf,const vector<Vector3> &vVector3Ds)
{
	SmtTinDelaunayDiv tinDela;
	tinDela.SetVertexs(vVector3Ds);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo3DSurf(p3DSurf))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}	

long CreateDelaunayTin_Div(SmtTin *pTin,const Vector3 *pVector3Ds,int nCount)
{
	SmtTinDelaunayDiv tinDela;
	tinDela.SetVertexs(pVector3Ds,nCount);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo2DTin(pTin))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

long CreateDelaunayTin_Div(SmtTin *pTin,const vector<Vector3> &vVector3Ds)
{
	SmtTinDelaunayDiv tinDela;
	tinDela.SetVertexs(vVector3Ds);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo2DTin(pTin))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}	

//////////////////////////////////////////////////////////////////////////
long CreateDelaunayTin_Inc(Smt3DSurface *p3DSurf,const Vector3 *pVector3Ds,int nCount)
{
	SmtTinDelaunayInc tinDela;
	tinDela.SetVertexs(pVector3Ds,nCount);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo3DSurf(p3DSurf))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

long CreateDelaunayTin_Inc(Smt3DSurface *p3DSurf,const vector<Vector3> &vVector3Ds)
{
	SmtTinDelaunayInc tinDela;
	tinDela.SetVertexs(vVector3Ds);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo3DSurf(p3DSurf))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

long CreateDelaunayTin_Inc(SmtTin *pTin,const Vector3 *pVector3Ds,int nCount)
{
	SmtTinDelaunayInc tinDela;
	tinDela.SetVertexs(pVector3Ds,nCount);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo2DTin(pTin))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

long CreateDelaunayTin_Inc(SmtTin *pTin,const vector<Vector3> &vVector3Ds)
{
	SmtTinDelaunayInc tinDela;
	tinDela.SetVertexs(vVector3Ds);

	if (SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo2DTin(pTin))
		return SMT_ERR_NONE;

	return SMT_ERR_FAILURE;
}

//////////////////////////////////////////////////////////////////////////
long	DivPolygenIntoTriMesh(vector<SmtTriangle> &trilist,dbfPoint *pPoints,int nPoint)
{
	//////////////////////////////////////////////////////////////////////////
	//准备数据
	vector<Vector3> vVector3Ds;
	vVector3Ds.resize(nPoint);
	for (int i = 0; i < nPoint;i++)
	{
		vVector3Ds[i].x = pPoints[i].x;
		vVector3Ds[i].y = pPoints[i].y;
		vVector3Ds[i].z = 0;
	}

	//构网
	SmtTin tin;
	SmtTinDelaunayDiv tinDela;
	tinDela.SetVertexs(vVector3Ds);

	if (!(SMT_ERR_NONE == tinDela.DoMesh() &&
		SMT_ERR_NONE == tinDela.CvtTo2DTin(&tin)))
		return SMT_ERR_NONE;

	//剔除
	SmtPoint oPt1,oPt2,oPt3;
	dbfPoint center;
	for (int i = 0; i < tin.GetTriangleCount();i++)
	{
		SmtTriangle tri = tin.GetTriangle(i);
		oPt1 = tin.GetPoint(tri.a);
		oPt2 = tin.GetPoint(tri.b);
		oPt3 = tin.GetPoint(tri.c);

		center.x = (oPt1.GetX()+oPt2.GetX()+oPt3.GetX())/3.;
		center.y = (oPt1.GetY()+oPt2.GetY()+oPt3.GetY())/3.;

		if (SmtPointToPolygon_New1(center,pPoints,nPoint) < 0)
			trilist.push_back(tri);
	}

	return SMT_ERR_NONE;
}

