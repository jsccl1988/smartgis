/*
File:    geo_3dapi.h

Desc:    SmartGis����3d Geom API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GEO_3D_API_H
#define _GEO_3D_API_H

#include "geo_geometry.h"
#include "geo_3dgeometry.h"
#include "ml3d_mathlib.h"

using namespace Smt_Geo;
using namespace Smt_3DGeo;
using namespace Smt_3DMath;

/************************************************************************/
/* 
����:��������Ĺ�ϵ
����:
���:
����:   -1 �ཻ /0 ���� /1����                                                           
*/
/************************************************************************/
long	SMT_EXPORT_API SmtRayToSphere(const Vector4& vOrg,const Vector4 &vDir,const Vector4 &vCenter,float fRaduis);

/************************************************************************/

/************************************************************************/
long	SMT_EXPORT_API SmtCvt3DSurfTo2DTin(const Smt3DSurface *p3DSurf,SmtTin *pTin);

#if     !defined(Export_Smt3DGeoCore)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DGeoCoreD.lib")
#       else
#          pragma comment(lib,"Smt3DGeoCore.lib")
#	    endif
#endif

#endif //_GEO_3D_API_H

