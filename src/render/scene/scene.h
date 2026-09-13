// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_SCENE_H_
#define RENDER_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "render/rhi/rhi.h"
#include "sdb/scene/scene.h"

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}

// GPU-resident instance list synced from sdb::scene::World.

namespace render {
namespace scene {

// One World node mirrored for GPU upload (layer / 3D geom pointers stay
// non-owning).
struct GpuInstance {
  uint64_t node_id;
  sdb::scene::NodeKind kind;
  double min_x;
  double min_y;
  double min_z;
  double max_x;
  double max_y;
  double max_z;
  const sdb::SmtLayer* layer = nullptr;
  OGRLayer* ogr_layer = nullptr;
  const OGRGeometry* geom_3d = nullptr;
  std::vector<const OGRGeometry*> geoms;
  const geo::Tin* tin = nullptr;
  const geo::Grid* grid = nullptr;
};

// Uploads tessellated OGRGeometry (2D and 3D instance Z) onto one list.
class GpuScene {
 public:
  GpuScene();
  ~GpuScene();
  GpuScene(const GpuScene&) = delete;
  GpuScene& operator=(const GpuScene&) = delete;

  uint64_t synced_generation() const { return synced_generation_; }
  size_t instance_count() const { return instances_.size(); }
  const GpuInstance* instance_at(size_t index) const;

  void sync_from(const sdb::scene::World& world);
  // Records raster / 2D / 3D passes. Does not close the list so leftover 3D
  // VB/IB can append on the same CommandList.
  bool record_draws(render::rhi::Device* device, render::rhi::CommandList* list,
                    uint32_t width, uint32_t height);
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height);
  void release();

  // GPU-uploaded triangle mesh for one World node (tessellated GIS geom).
  struct GpuMesh {
    sdb::scene::NodeKind kind;
    render::rhi::Buffer* vertex;
    render::rhi::Buffer* index;
    render::rhi::Texture* texture;
    uint32_t index_count;
    uint32_t stride;
  };

 private:
  void clear_meshes();
  bool rebuild_meshes(render::rhi::Device* device);

  uint64_t synced_generation_;
  std::vector<GpuInstance> instances_;
  render::rhi::Device* upload_device_;
  std::vector<GpuMesh> meshes_;
  bool meshes_dirty_;
};

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_SCENE_H_
