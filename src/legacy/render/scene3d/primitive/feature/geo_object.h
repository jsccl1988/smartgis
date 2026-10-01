// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
#define SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_

#include "legacy/gis/feature/mesh.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/device/render_device.h"
#include "legacy/render/rhi3d/public/resource/video_buffer.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {

// Frame for leftover feature tessellation (see legacy/gis/feature).
enum class GeoObjectFrame : unsigned char {
  kMap = 0,    // lon/lat + optional DEM drape + SmtStyle colors
  kWorld = 1,  // true 3D OGR coords → VB (X,Z,Y)
};

// Merged leftover 2D/3D geo drawable: tess in gis.dll, VB upload here.
class LEGACY_RENDER_EXPORT SmtGeoObject : public Smt3DObject {
 public:
  SmtGeoObject();
  ~SmtGeoObject() override;

  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

  OGRGeometry* GetGeometryRef() { return geom_; }
  void SetGeometryDirectly(OGRGeometry* pGeom);
  void SetGeometry(OGRGeometry* pGeom);
  void SetStyle(const SmtStyle* pStyle);

  void set_frame(GeoObjectFrame frame) { frame_ = frame; }
  GeoObjectFrame frame() const { return frame_; }

  void SetHeightSampleFn(FeatureHeightSampleFn fn, void* user);

  // Upload a pre-tessellated mesh (used to batch many map lines into one draw).
  long CreateFromMesh(LP3DRENDERDEVICE device, FeatureMesh mesh);

  SmtGeoObject* Clone();

 private:
  bool upload_mesh(LP3DRENDERDEVICE device, const FeatureMesh& mesh);
  ulong vertex_format(const FeatureMesh& mesh) const;
  void update_aabb_map();
  void update_aabb_world();
  void update_aabb_from_mesh(const FeatureMesh& mesh);

  SmtVertexBuffer* vb_ = nullptr;
  SmtIndexBuffer* ib_ = nullptr;
  OGRGeometry* geom_ = nullptr;
  SmtStyle* style_ = nullptr;
  FeatureHeightSampleFn height_fn_ = nullptr;
  void* height_user_ = nullptr;
  GeoObjectFrame frame_ = GeoObjectFrame::kMap;
  FeaturePrim prim_ = FeaturePrim::kPoints;
  bool indexed_ = false;
};

}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
