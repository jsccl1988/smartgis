// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_SCENE_SCENE_H_
#define RENDER_SCENE_SCENE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "render/render_export.h"
#include "render/rhi/rhi.h"
#include "sdb/scene/scene.h"
#include "sdb/style/style_types.h"

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}

// GPU-resident instance list synced from sdb::scene::World.

namespace render {
namespace scene {

// Maps ResolvedPaint layer type to RGBA. Multiplies the matching opacity into
// alpha. kBackground / kFill / unknown → fill_*; kLine → line_*; kCircle →
// circle_*; kSymbol without bytes still returns fill_* as a fallback tint.
RENDER_EXPORT void rgba_from_resolved_paint(
    const sdb::style::ResolvedPaint& paint, float* r, float* g, float* b,
    float* a);

// One World node mirrored for GPU upload (layer / 3D geom pointers stay
// non-owning). Optional ResolvedPaint is applied after sync_from.
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
  bool has_paint = false;
  sdb::style::ResolvedPaint paint;
};

// Uploads tessellated OGRGeometry (2D and 3D instance Z) onto one list.
class RENDER_EXPORT GpuScene {
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
  // Marks meshes dirty: stroke/circle world size depends on world_units_per_pixel.
  void set_view_ortho(double min_x, double min_y, double max_x, double max_y);
  void clear_view_ortho();
  bool has_view_ortho() const { return view_ortho_set_; }

  // Default solid fill for untextured meshes without per-instance paint
  // (matches leftover brush cyan).
  void set_solid_color(float r, float g, float b, float a);
  void set_solid_color_from_colorref(long colorref);

  // Attach Style paint to one synced instance. Does not parse StyleDocument;
  // callers resolve via sdb::style first. Marks meshes dirty.
  bool set_instance_paint(size_t index, const sdb::style::ResolvedPaint& paint);
  void clear_instance_paint(size_t index);

  // Solid clear color from a background layer paint (fill_color × fill_opacity).
  void set_background_paint(const sdb::style::ResolvedPaint& paint);
  void clear_background_paint();
  bool has_background_paint() const { return background_set_; }

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
    // Style scalars applied at tessellate time (line ribbon / circle diamond).
    float line_width;
    float circle_radius;
  };

  // Uploaded mesh inspection (null-device tests / debug).
  size_t mesh_count() const { return meshes_.size(); }
  const GpuMesh* mesh_at(size_t index) const;

 private:
  void clear_meshes();
  // Viewport size drives world_units_per_pixel for paint-aware line/circle
  // tessellation (pixel_width / circle_radius → world units).
  bool rebuild_meshes(render::rhi::Device* device, uint32_t width,
                      uint32_t height);
  void resolve_view_envelope(uint32_t width, uint32_t height, double* min_x,
                             double* min_y, double* max_x,
                             double* max_y) const;

  uint64_t synced_generation_;
  std::vector<GpuInstance> instances_;
  render::rhi::Device* upload_device_;
  std::vector<GpuMesh> meshes_;
  bool meshes_dirty_;
  uint32_t upload_width_;
  uint32_t upload_height_;
  bool view_ortho_set_;
  double view_min_x_;
  double view_min_y_;
  double view_max_x_;
  double view_max_y_;
  float solid_r_;
  float solid_g_;
  float solid_b_;
  float solid_a_;
  bool background_set_;
  float background_r_;
  float background_g_;
  float background_b_;
  float background_a_;
};

}  // namespace scene
}  // namespace render

#endif  // RENDER_SCENE_SCENE_H_
