// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_

#include <string_view>
#include <vector>

#include "scenic/render/scenic_impl_export.h"
#include "scenic/scene3d/primitive/surface/surface_base.h"
#include "scenic/scene3d/scene/vertex3d.h"

namespace scenic {
namespace detail {

// One contiguous VB range for frustum-culled point drawing.
struct PointCloudChunk {
  Aabb aabb;
  ulong start = 0;
  ulong count = 0;
};

// Point cloud: owns VB and optional spatial chunks for frustum cull.
class SCENIC_IMPL_EXPORT PointCloud3d : public SurfaceObject {
 public:
  PointCloud3d();
  ~PointCloud3d() override;

  long Init(::base::Vector3& pos, Material& material,
            const char* tex_name = "") override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Create(LP3DRENDERDEVICE device) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;

  bool show_bounds() const { return show_bounds_; }
  void set_show_bounds(bool show) { show_bounds_ = show; }

  bool read_point_cloud(std::string_view path);
  int last_drawn_points() const { return last_drawn_points_; }

 private:
  long build_gpu_buffer(LP3DRENDERDEVICE device);
  void build_chunks(const std::vector<Vertex3d>& packed);
  void pack_vertices_for_chunks(std::vector<Vertex3d>* packed);

  std::vector<PointCloudChunk> chunks_;
  std::vector<Vertex3d> vertices_;
  bool show_bounds_ = true;
  bool read_ok_ = false;
  int last_drawn_points_ = 0;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
