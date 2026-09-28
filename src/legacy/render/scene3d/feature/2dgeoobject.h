// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _MD3D_2DGEOOBJECT_H
#define _MD3D_2DGEOOBJECT_H

#include "base/carto/style.h"
#include "gis/kernel/geo/mesh/geometry.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/3drenderdevice.h"
#include "legacy/render/rhi3d/public/device/3drenderer.h"
#include "legacy/render/rhi3d/public/resource/videobuffer.h"
#include "legacy/render/scene3d/scene/object.h"

using namespace render;
using namespace base;
using namespace geo;

namespace render {
class LEGACY_RENDER_EXPORT Smt2DGeoObject : public Smt3DObject {
 public:
  Smt2DGeoObject(void);
  virtual ~Smt2DGeoObject();

 public:
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
  void SetStyle(const SmtStyle *pStyle);

  using HeightSampleFn = float (*)(double x, double y, void *user);
  void SetHeightSampleFn(HeightSampleFn fn, void *user);

  Smt2DGeoObject *Clone();

 protected:
  // create
  bool CreatePointVB(LP3DRENDERDEVICE p3DRenderDevice, OGRPoint *pPoint);
  bool CreateMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice,
                          OGRMultiPoint *pMultPoint);
  bool CreateLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                          OGRLineString *pLineString);
  bool CreateSplineVB(LP3DRENDERDEVICE p3DRenderDevice, OGRLineString *pSpline);
  bool CreateLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice,
                          OGRLinearRing *pLinearRing);
  bool CreateMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice,
                               OGRMultiLineString *pMultLinearRing);
  bool CreatePolygonVB(LP3DRENDERDEVICE p3DRenderDevice, OGRPolygon *pPoly);
  bool CreateMultiPolygonVB(LP3DRENDERDEVICE p3DRenderDevice,
                            OGRMultiPolygon *pMulti);

  // render
  bool RenderPointVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderMultiPointVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderSplineVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderLinearRingVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderMultiLineStringVB(LP3DRENDERDEVICE p3DRenderDevice);
  bool RenderPolygonVB(LP3DRENDERDEVICE p3DRenderDevice);

 private:
  ulong vertex_format() const;
  float height_at(double x, double y) const;
  void emit_map_vertex(SmtVertexBuffer *vb, double x, double y, float r,
                       float g, float b);

  SmtVertexBuffer *m_pVertexBuffer;
  SmtIndexBuffer *m_pIndexBuffer;
  OGRGeometry *m_pGeom;
  SmtStyle *m_pStyle;
  HeightSampleFn m_height_fn;
  void *m_height_user;
};
}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_MD3D_2DGEOOBJECT_H
