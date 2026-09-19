/*
File:   md3d_3dgeometry.h

Desc:    Smt3DGeoObject,3d���ζ���

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_3DGEOOBJECT_H
#define _MD3D_3DGEOOBJECT_H

#include "algorithm/geo/geometry.h"
#include "legacy/render/render3d/3drenderdevice.h"
#include "legacy/render/render3d/3drenderer.h"
#include "legacy/render/render3d/videobuffer.h"
#include "legacy/render/scene3d/bl3d_object.h"

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
using namespace geo;

namespace render {
class MODEL3D_EXPORT_CLASS Smt3DGeoObject : public Smt3DObject {
 public:
  Smt3DGeoObject(void);
  virtual ~Smt3DGeoObject();

 public:
  //
  long Init(Vector3 &vPos, SmtMaterial &matMaterial,
            const char *szTexName = "");
  long Create(LP3DRENDERDEVICE p3DRenderDevice);
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed);
  long Render(LP3DRENDERDEVICE p3DRenderDevice);
  long Destroy();

 public:
  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint &point);

 public:
  inline OGRGeometry *GetGeometryRef(void) { return m_pGeom; }
  void SetGeometryDirectly(OGRGeometry *pGeom);
  void SetGeometry(OGRGeometry *pGeom);

  Smt3DGeoObject *Clone();

 protected:
  // create
  bool Create3DPointVB(LP3DRENDERDEVICE p3DRenderDevice, OGRPoint *p3DPoint);
  bool Create3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,
                            OGRMultiPoint *p3DMultPoint);
  bool Create3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                            OGRLineString *p3DLineString);
  bool Create3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,
                            OGRLinearRing *p3DLinearRing);
  bool Create3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                                 OGRMultiLineString *p3DMultLinearRing);
  bool Create3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice,
                         Smt3DSurface *p3DSurf);

  // render
  bool Render3DPointVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool Render3DMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool Render3DLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool Render3DLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool Render3DMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool Render3DSurfaceVB(LP3DRENDERDEVICE p3DRenderDevice);

 private:
  SmtVertexBuffer *m_pVertexBuffer;
  SmtIndexBuffer *m_pIndexBuffer;
  OGRGeometry *m_pGeom;
};
}  // namespace render

#if !defined(MODEL3D_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_3DGEOOBJECT_H
