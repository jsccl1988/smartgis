// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
#define SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_

#include <memory>

#include "scenic/scene3d/primitive/feature/feature_mesh.h"
#include "scenic/scene3d/primitive/mesh/mesh_gpu.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/render/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_device.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

// Frame for leftover feature tessellation.
enum class GeoObjectFrame : unsigned char {
  kMap = 0,    // lon/lat + optional DEM drape + Style colors
  kWorld = 1,  // true 3D OGR coords → VB (X,Z,Y)
};

// Merged leftover 2D/3D geo drawable: tess in gis.dll, VB upload here.
class SCENIC_IMPL_EXPORT GeoObject : public Object3d {
 public:
  GeoObject();
  ~GeoObject() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Create(LP3DRENDERDEVICE device) override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;
  bool Select(LP3DRENDERDEVICE device, const lPoint& point) override;

  OGRGeometry* geometry() { return geom_; }
  void set_geometry_directly(OGRGeometry* geom);
  void set_geometry(OGRGeometry* geom);
  void set_style(const Style* style);

  void set_frame(GeoObjectFrame frame) { frame_ = frame; }
  GeoObjectFrame frame() const { return frame_; }

  void set_height_sample(FeatureHeightSampleFn fn, void* user);

  long create_from_mesh(LP3DRENDERDEVICE device, FeatureMesh mesh);

  GeoObject* clone();

 private:
  bool upload_mesh(LP3DRENDERDEVICE device, const FeatureMesh& mesh);
  ulong vertex_format(const FeatureMesh& mesh) const;
  void update_aabb_map();
  void update_aabb_world();
  void update_aabb_from_mesh(const FeatureMesh& mesh);

  GpuVertexBuffer vb_;
  GpuIndexBuffer ib_;
  OGRGeometry* geom_ = nullptr;
  std::unique_ptr<Style> style_;
  FeatureHeightSampleFn height_fn_ = nullptr;
  void* height_user_ = nullptr;
  GeoObjectFrame frame_ = GeoObjectFrame::kMap;
  FeaturePrim prim_ = FeaturePrim::kPoints;
  bool indexed_ = false;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_FEATURE_GEO_OBJECT_H_
