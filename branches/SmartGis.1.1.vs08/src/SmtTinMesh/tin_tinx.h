/*
File:   tin_tinx.h

Desc:    SmtTinX,Èý½ÇÍø»ùÀà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _TIN_TINX_H
#define _TIN_TINX_H

#include "smt_core.h"
#include "geo_geometry.h"
#include "geo_3dgeometry.h"

using namespace Smt_Core;
using namespace Smt_Geo;
using namespace Smt_3DGeo;

namespace Smt_TinMesh
{

#ifdef TIN_ALT_SINGLE
	#define real float
#else
	#define real double
#endif

	class SMT_EXPORT_CLASS SmtTinX
	{
	public:
		SmtTinX(void) {}
		virtual ~SmtTinX(void) {}

	public:
		inline float			GetMaxX(){return m_fMaxX;}
		inline float			GetMinX(){return m_fMinX;}

		inline float			GetMaxY(){return m_fMaxY;}
		inline float			GetMinY(){return m_fMinY;}

		inline float			GetMaxZ(){return m_fMaxZ;}
		inline float			GetMinZ(){return m_fMinZ;}

		//1.
		virtual long			SetVertexs(const Vector3 *pVector3Ds,int nCount) = 0;
		virtual long			SetVertexs(const vector<Vector3> &vVector3Ds) = 0;

		//2.
		virtual	long			DoMesh(void) = 0;

		//3.
		virtual long			CvtTo3DSurf(Smt3DSurface *p3DSurf) = 0;		//3d
		virtual long			CvtTo2DTin(SmtTin *pTin) = 0;				//2d
		
	protected:
		float					m_fMinX;
		float					m_fMaxX;

		float					m_fMinY;
		float					m_fMaxY;

		float					m_fMinZ;
		float					m_fMaxZ;
	};
}

#if !defined(Export_SmtTinMesh)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtTinMeshD.lib")
#       else
#          pragma comment(lib,"SmtTinMesh.lib")
#	    endif  
#endif

#endif //_TIN_TINX_H