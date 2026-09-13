// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_SCENE_H_
#define RENDER_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
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
  const sdb::model::ModelAsset* model = nullptr;
  const sdb::model::Tileset* tileset = nullptr;
  std::vector<std::string> visible_uris;
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

  // Ortho zoom/pan override (map envelope). When unset, AABB of 2D instances.
  void set_view_ortho(double min_x, double min_y, double max_x, double max_y);
  void clear_view_ortho();
  bool has_view_ortho() const { return view_ortho_set_; }

  // Default solid fill for untextured meshes (matches leftover brush cyan).
  void set_solid_color(float r, float g, float b, float a);
  void set_solid_color_from_colorref(long colorref);

  // GPU-uploaded triangle mesh for one World node (tessellated GIS geom).
  struct GpuMesh {
    sdb::scene::NodeKind kind;
    render::rhi::Buffer* vertex;
    render::rhi::Buffer* index;
    render::rhi::Texture* texture;
    uint32_t index_count;
    uint32_t stride;
    float solid_r;
    float solid_g;
    float solid_b;
    float solid_a;
  };

 private:
  void clear_meshes();
  bool rebuild_meshes(render::rhi::Device* device);

  uint64_t synced_generation_;
  std::vector<GpuInstance> instances_;
  render::rhi::Device* upload_device_;
  std::vector<GpuMesh> meshes_;
  bool meshes_dirty_;
  bool view_ortho_set_;
  double view_min_x_;
  double view_min_y_;
  double view_max_x_;
  double view_max_y_;
  float solid_r_;
  float solid_g_;
  float solid_b_;
  float solid_a_;
};

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_SCENE_H_
