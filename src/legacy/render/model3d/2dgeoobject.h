/*
File:   md3d_2dgeometry.h

Desc:    Smt2DGeoObject,2d���ζ���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_2DGEOOBJECT_H
#define _MD3D_2DGEOOBJECT_H

#include "legacy/render/render3d/3drenderer.h"
#include "legacy/render/render3d/videobuffer.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/scene3d/bl3d_object.h"
#include "algorithm/geo/geometry.h"
#include "sdb/carto/style.h"

#if !defined(MODEL3D_EXPORT_DEFINED)
#define MODEL3D_EXPORT_DEFINED
#if defined(MODEL3D_EXPORTS)
#define MODEL3D_EXPORT_API __declspec(dllexport)
#define MODEL3D_EXPORT_CLASS __declspec(dllexport)
#else
#define MODEL3D_EXPORT_API __declspec(dllimport)
#define MODEL3D_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

using namespace render;
using namespace base;
using namespace geo;

namespace render
{
	class MODEL3D_EXPORT_CLASS Smt2DGeoObject :public Smt3DObject
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
		inline OGRGeometry		*GetGeometryRef(void) {return m_pGeom;}
		void                     SetGeometryDirectly(OGRGeometry  *pGeom);
		void                     SetGeometry(OGRGeometry  *pGeom);
		void                     SetStyle(const SmtStyle * pStyle);

		Smt2DGeoObject			*Clone();

	protected:
		//create
		bool					CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice,OGRPoint *pPoint);
		bool					CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,OGRMultiPoint *pMultPoint);
		bool					CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLineString *pLineString);
		bool					CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLineString *pSpline);
		bool					CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,OGRLinearRing *pLinearRing);
		bool					CreateMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,OGRMultiLineString *pMultLinearRing);
		bool					CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice,OGRPolygon *pPoly);
		bool					CreateMultiPolygonVB(LP3DRENDERDEVICE p3DRenderDevice,OGRMultiPolygon *pMulti);

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
		OGRGeometry				*m_pGeom; 
		SmtStyle				*m_pStyle;
	};
}

#if     !defined(MODEL3D_EXPORTS)
#if     defined(_DEBUG)
#          pragma comment(lib,"legacy_render_d.lib")
#       else
#          pragma comment(lib,"legacy_render.lib")
#	    endif
#endif

#endif //_MD3D_2DGEOOBJECT_H
