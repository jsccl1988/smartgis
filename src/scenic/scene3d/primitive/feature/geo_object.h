// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
#define SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_

#include "scenic/detail/feature_mesh.h"
#include "scenic/detail/style.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/render/rhi3d/public/resource/video_buffer.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// mesh.h types live in namespace render; scenic TUs are scenic::detail.
using render::FeatureHeightSampleFn;
using render::FeatureMesh;
using render::FeaturePrim;
using render::FeatureRgb;
using render::FeatureVertex;

// Frame for leftover feature tessellation (see legacy/gis/feature).
enum class GeoObjectFrame : unsigned char {
  kMap = 0,    // lon/lat + optional DEM drape + Style colors
  kWorld = 1,  // true 3D OGR coords �?VB (X,Z,Y)
};

// Merged leftover 2D/3D geo drawable: tess in gis.dll, VB upload here.
class LEGACY_RENDER_EXPORT GeoObject : public Object3d {
 public:
  GeoObject();
  ~GeoObject() override;

  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  bool Select(LP3DRENDERDEVICE p3DRenderDevice, const lPoint& point);

  OGRGeometry* GetGeometryRef() { return geom_; }
  void SetGeometryDirectly(OGRGeometry* pGeom);
  void SetGeometry(OGRGeometry* pGeom);
  void SetStyle(const Style* pStyle);

  void set_frame(GeoObjectFrame frame) { frame_ = frame; }
  GeoObjectFrame frame() const { return frame_; }

  void SetHeightSampleFn(FeatureHeightSampleFn fn, void* user);

  // Upload a pre-tessellated mesh (used to batch many map lines into one draw).
  long CreateFromMesh(LP3DRENDERDEVICE device, FeatureMesh mesh);

  GeoObject* Clone();

 private:
  bool upload_mesh(LP3DRENDERDEVICE device, const FeatureMesh& mesh);
  ulong vertex_format(const FeatureMesh& mesh) const;
  void update_aabb_map();
  void update_aabb_world();
  void update_aabb_from_mesh(const FeatureMesh& mesh);

  VertexBuffer* vb_ = nullptr;
  IndexBuffer* ib_ = nullptr;
  OGRGeometry* geom_ = nullptr;
  Style* style_ = nullptr;
  FeatureHeightSampleFn height_fn_ = nullptr;
  void* height_user_ = nullptr;
  GeoObjectFrame frame_ = GeoObjectFrame::kMap;
  FeaturePrim prim_ = FeaturePrim::kPoints;
  bool indexed_ = false;
};

}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  // SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
