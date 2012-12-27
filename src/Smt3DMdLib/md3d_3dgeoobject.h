/*
File:   md3d_3dgeometry.h

Desc:    Smt3DGeoObject,3d几何对象

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_3DGEOOBJECT_H
#define _MD3D_3DGEOOBJECT_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"
#include "geo_3dgeometry.h"

using namespace Smt_3DBase;
using namespace Smt_3DGeo;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS Smt3DGeoObject :public Smt3DObject
	{
	public:
		Smt3DGeoObject(void);
		virtual~Smt3DGeoObject();

	public:
		//
		long					Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName = "");
		long					Create(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long					Render(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Destroy();

	public:
		bool					Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point);

	public:
		inline Smt3DGeometry	*GetGeometryRef(void) {return m_pGeom;}
		void                     SetGeometryDirectly(Smt3DGeometry  *pGeom);
		void                     SetGeometry(Smt3DGeometry  *pGeom);

		Smt3DGeoObject			*Clone();

	protected:
		//create
		bool					Create3DPointVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DPoint *p3DPoint);
		bool					Create3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DMultiPoint *p3DMultPoint);
		bool					Create3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DLineString *p3DLineString);
		bool					Create3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DLinearRing *p3DLinearRing);
		bool					Create3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DMultiLineString *p3DMultLinearRing);
		bool					Create3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice,Smt3DSurface *p3DSurf);

		//render
		bool					Render3DPointVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					Render3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					Render3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					Render3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					Render3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					Render3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice);

	private:
		SmtVertexBuffer			*m_pVertexBuffer;
		SmtIndexBuffer			*m_pIndexBuffer;
		Smt3DGeometry			*m_pGeom; 
	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif

#endif //_MD3D_3DGEOOBJECT_H
