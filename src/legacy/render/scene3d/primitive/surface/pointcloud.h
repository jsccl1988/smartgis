// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
#define SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_

#include <vector>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/index/vertex_octree.h"
#include "legacy/render/scene3d/primitive/surface/surface_base.h"

namespace render {

// One contiguous VB range for frustum-culled point drawing.
struct PointCloudChunk {
  Aabb aabb;
  ulong start = 0;
  ulong count = 0;
};

// Leftover point cloud: owns VB (+ optional spatial chunks); unibn index is
// query-only via SmtVertexOctTree.
class LEGACY_RENDER_EXPORT Smt3DPointCloud : public SmtSurfaceObject {
 public:
  Smt3DPointCloud();
  ~Smt3DPointCloud() override;

  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
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

  SmtVertexOctTree& point_index() { return point_index_; }
  const SmtVertexOctTree& point_index() const { return point_index_; }
  SmtVertexOctTree& GetVertexOctTree() { return point_index_; }

  int last_drawn_points() const { return last_drawn_points_; }

 private:
  long build_gpu_buffer(LP3DRENDERDEVICE device);
  void build_chunks(const SmtVertex3DList& packed);
  void pack_vertices_for_chunks(SmtVertex3DList* packed);

  SmtVertexOctTree point_index_;
  std::vector<PointCloudChunk> chunks_;
  SmtVertex3DList vtx_list_;
  bool show_bounds_ = true;
  bool read_ok_ = false;
  int last_drawn_points_ = 0;
};

}  // namespace render

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  // SMT_LEGACY_RENDER_SCENE3D_PRIMITIVE_SURFACE_POINTCLOUD_H_
