/*
File:   tin_api.h

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _TIN_API_H
#define _TIN_API_H

#include "smt_core.h"
#include "geo_geometry.h"
#include "geo_3dgeometry.h"

using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_3DGeo;

//////////////////////////////////////////////////////////////////////////
long		SMT_EXPORT_API		CreateDelaunayTin_Div(Smt3DSurface *p3DSurf,const Vector3 *pVector3Ds,int nCount);
long		SMT_EXPORT_API		CreateDelaunayTin_Div(Smt3DSurface *p3DSurf,const vector<Vector3> &vVector3Ds);

long		SMT_EXPORT_API		CreateDelaunayTin_Div(SmtTin *pTin,Vector3 *pVector3Ds,int nCount);
long		SMT_EXPORT_API		CreateDelaunayTin_Div(SmtTin *pTin,const vector<Vector3> &vVector3Ds);

//////////////////////////////////////////////////////////////////////////
long		SMT_EXPORT_API		CreateDelaunayTin_Inc(Smt3DSurface *p3DSurf,const Vector3 *pVector3Ds,int nCount);
long		SMT_EXPORT_API		CreateDelaunayTin_Inc(Smt3DSurface *p3DSurf,const vector<Vector3> &vVector3Ds);

long		SMT_EXPORT_API		CreateDelaunayTin_Inc(SmtTin *pTin,const Vector3 *pVector3Ds,int nCount);
long		SMT_EXPORT_API		CreateDelaunayTin_Inc(SmtTin *pTin,const vector<Vector3> &vVector3Ds);

//////////////////////////////////////////////////////////////////////////
long		SMT_EXPORT_API		DivPolygenIntoTriMesh(vector<SmtTriangle> &trilist,dbfPoint *pPoints,int nPoint);

#if !defined(Export_SmtTinMesh)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtTinMeshD.lib")
#       else
#          pragma comment(lib,"SmtTinMesh.lib")
#	    endif  
#endif

#endif //_TIN_API_H