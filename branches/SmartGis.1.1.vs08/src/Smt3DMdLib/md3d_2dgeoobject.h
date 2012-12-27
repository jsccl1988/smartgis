/*
File:   md3d_2dgeometry.h

Desc:    Smt2DGeoObject,2d几何对象

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_2DGEOOBJECT_H
#define _MD3D_2DGEOOBJECT_H

#include "rd3d_3drenderer.h"
#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"
#include "geo_geometry.h"
#include "bl_style.h"

using namespace Smt_3DBase;
using namespace Smt_Base;
using namespace Smt_Geo;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS Smt2DGeoObject :public Smt3DObject
	{
	public:

		Smt2DGeoObject(void);
		virtual~Smt2DGeoObject();

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
		inline SmtGeometry		*GetGeometryRef(void) {return m_pGeom;}
		void                     SetGeometryDirectly(SmtGeometry  *pGeom);
		void                     SetGeometry(SmtGeometry  *pGeom);
		void                     SetStyle(const SmtStyle * pStyle);

		Smt2DGeoObject			*Clone();

	protected:
		//create
		bool					CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice,SmtPoint *pPoint);
		bool					CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,SmtMultiPoint *pMultPoint);
		bool					CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,SmtLineString *pLineString);
		bool					CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice,SmtSpline *pSpline);
		bool					CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,SmtLinearRing *pLinearRing);
		bool					CreateMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,SmtMultiLineString *pMultLinearRing);
		bool					CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice,SmtPolygon *pPoly);

		//render
		bool					RenderPointVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderSplineVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
		bool					RenderPolygonVB(LP3DRENDERDEVICE p3DRenderDevice);

	private:
		SmtVertexBuffer			*m_pVertexBuffer;
		SmtIndexBuffer			*m_pIndexBuffer;
		SmtGeometry				*m_pGeom; 
		SmtStyle				*m_pStyle;
	};
}

#if     !defined(Export_Smt3DMdLib)
#if     defined(_DEBUG)
#          pragma comment(lib,"Smt3DMdLibD.lib")
#       else
#          pragma comment(lib,"Smt3DMdLib.lib")
#	    endif
#endif

#endif //_MD3D_2DGEOOBJECT_H
