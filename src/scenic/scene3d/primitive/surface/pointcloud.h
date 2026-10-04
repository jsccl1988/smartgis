// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
#define SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_

#include <vector>

#include "scenic/scenic_impl_export.h"
#include "scenic/scene3d/index/vertex_octree.h"
#include "scenic/scene3d/primitive/surface/surface_base.h"

namespace scenic {
namespace detail {

// One contiguous VB range for frustum-culled point drawing.
struct PointCloudChunk {
  Aabb aabb;
  ulong start = 0;
  ulong count = 0;
};

// Leftover point cloud: owns VB (+ optional spatial chunks); unibn index is
// query-only via VertexOctTree.
class LEGACY_RENDER_EXPORT PointCloud3d : public SurfaceObject {
 public:
  PointCloud3d();
  ~PointCloud3d() override;

  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "") override;
  long Update(LP3DRENDERDEVICE device, float elapsed) override;
  long Create(LP3DRENDERDEVICE device) override;
  long Render(LP3DRENDERDEVICE device) override;
  long Destroy() override;

  bool show_bounds() const { return show_bounds_; }
  void set_show_bounds(bool show) { show_bounds_ = show; }
  // Legacy aliases.
  bool GetShowOctNodeBox() { return show_bounds_; }
  void SetShowOctNodeBox(bool show = true) { show_bounds_ = show; }

  bool Read3DPointCloud(const char* path);
  bool read_point_cloud(const char* path) { return Read3DPointCloud(path); }

  VertexOctTree& point_index() { return point_index_; }
  const VertexOctTree& point_index() const { return point_index_; }
  VertexOctTree& GetVertexOctTree() { return point_index_; }

  int last_drawn_points() const { return last_drawn_points_; }

 private:
  long build_gpu_buffer(LP3DRENDERDEVICE device);
  void build_chunks(const Vertex3dList& packed);
  void pack_vertices_for_chunks(Vertex3dList* packed);

  VertexOctTree point_index_;
  std::vector<PointCloudChunk> chunks_;
  Vertex3dList vtx_list_;
  bool show_bounds_ = true;
  bool read_ok_ = false;
  int last_drawn_points_ = 0;
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

#endif  // SCENIC_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
