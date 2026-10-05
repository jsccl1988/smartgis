// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_PASS_H_
#define VISTA_PASS_WORLD_PASS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gis/style/style_types.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/vista_export.h"
#include "vista/component/world/instance.h"
#include "vista/component/world/world.h"
#include "vista/pass/world/gpu_mesh.h"
#include "vista/pass/world/terrain/pass.h"

namespace vista {

// Device-thread recorder for one World. sync_from copies CPU state;
// record_draws uploads and records. Content keeps one long-lived object.
// Upload / tint / per-kind draw live under detail/; this type owns the public
// sync → ensure → record order.
class VISTA_EXPORT WorldPass {
 public:
  using GpuMesh = vista::GpuMesh;

  WorldPass();
  ~WorldPass();
  WorldPass(const WorldPass&) = delete;
  WorldPass& operator=(const WorldPass&) = delete;

  uint64_t synced_generation() const { return synced_generation_; }
  size_t instance_count() const { return instances_.size(); }
  const Instance* instance_at(size_t index) const;

  void sync_from(const vista::World& world);
  // Emits meshes and does not close the list, so leftover 3D VB/IB can append.
  // When |bound_camera| is non-null, the caller
  // already bound that RecordContext camera: this does not call bind_camera
  // and does not choose ortho versus perspective. Frustum cull uses
  // |bound_camera|. When it is null and no view camera is set, a temporary
  // legacy path binds ortho for raster and vectors, then perspective if a
  // 3D instance is present.
  bool record_draws(render::rhi::Device* device, render::rhi::CommandList* list,
                    uint32_t width, uint32_t height,
                    const render::rhi::CameraMatrices* bound_camera = nullptr);
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height);
  // Destroy uploaded buffers via the upload Device (Device must still be live).
  void release();
  // Drop mesh handles without Device::destroy_* (Device already gone / Null
  // stub leak policy). Prefer release() when the Device is still valid.
  void abandon();

  // Force rebuild_meshes on the next record (e.g. after ocean height alloc
  // recycles FlyCube texture heap that still backs DEM albedo SRVs).
  void mark_meshes_dirty() { meshes_dirty_ = true; }

  // Upload / rebuild GPU meshes now (before recording other passes). Needed
  // when AtmosphereFrame opens sky/depth passes first — creating DEM albedo
  // mid-list left samples black (21,0,0) despite green CPU bake.
  bool ensure_meshes(render::rhi::Device* device, uint32_t width,
                     uint32_t height);

  // Compile solid+textured (and lit only when needed). Safe to call before
  // ensure_meshes so present can attribute pso_ms separately.
  bool warm_pipelines(render::rhi::Device* device);

  // True when the next ensure_meshes / record_draws will call rebuild_meshes.
  bool needs_mesh_upload(render::rhi::Device* device, uint32_t width,
                         uint32_t height) const;

  // Marks meshes dirty. rebuild_meshes uses this envelope to compute
  // world_units_per_pixel. Not a second projection.
  void set_view_ortho(double min_x, double min_y, double max_x, double max_y);
  void clear_view_ortho();
  bool has_view_ortho() const { return view_ortho_set_; }

  // Optional view camera for callers that do not pass a RecordContext camera
  // into record_draws. When set and no bound camera was passed, record_draws
  // binds it once and does not switch projection.
  void set_view_camera(const render::rhi::CameraMatrices& camera);
  void clear_view_camera();
  bool has_view_camera() const { return view_camera_set_; }

  // Default solid fill for untextured meshes without per-instance paint
  // (matches leftover brush cyan). Draw-time ColorCB only — does not mark
  // meshes dirty / re-upload (patches existing GpuMesh tint in place).
  void set_solid_color(float r, float g, float b, float a);
  void set_solid_color_from_colorref(long colorref);

  // Sky-on / stereo gate for FlyCube DEM: when |gate| is true, force solid
  // terrain tint from the largest synced hypso bake (textured samples read
  // near-black after ocean/sky SRV alloc). |gate| false clears the force flag.
  // Forwards to composed TerrainPass.
  void update_solid_terrain(uint64_t generation, bool gate);
  bool solid_terrain_forced() const { return terrain_.solid_terrain_forced(); }
  void clear_solid_terrain_cache();

  // Attach Style paint to one synced instance. Does not parse StyleDocument;
  // callers resolve via gis::style first. Marks meshes dirty.
  bool set_instance_paint(size_t index, const gis::style::ResolvedPaint& paint);
  void clear_instance_paint(size_t index);

  // Solid clear color from a background layer paint (fill_color × fill_opacity).
  void set_background_paint(const gis::style::ResolvedPaint& paint);
  void clear_background_paint();
  bool has_background_paint() const { return background_set_; }

  // When composing after an atmosphere clear (ocean sky), set kLoad so land
  // begin_render_pass markers do not replace prior color.
  void set_color_load_op(render::rhi::ColorLoadOp op) { color_load_op_ = op; }
  render::rhi::ColorLoadOp color_load_op() const { return color_load_op_; }

  // Draw terrain / TIN triangle edges after the filled pass (GDI overlay uses
  // the same flag on Scene3dController). Default off.
  void set_wireframe(bool on) { wireframe_ = on; }
  bool wireframe() const { return wireframe_; }

  // Directional light for lit / lit-textured DEM (defaults match Light{}).
  void set_light(const render::programs::Light& light) { light_ = light; }
  const render::programs::Light& light() const { return light_; }

  // Optional LRU of decoded 3D Tiles content. When set, kTileset rebuild
  // prefers cache hits over decode_content_file (present/orbit stream path).
  void set_tileset_content_cache(vista::TilesetContentCache* cache) {
    tileset_content_cache_ = cache;
  }
  vista::TilesetContentCache* tileset_content_cache() const {
    return tileset_content_cache_;
  }

  // Attach the shared depth buffer (ocean → land → clouds). First pass clears;
  // later callers should set DepthLoadOp::kLoad via set_depth_load_op.
  void set_enable_depth(bool on) { enable_depth_ = on; }
  bool enable_depth() const { return enable_depth_; }
  void set_depth_load_op(render::rhi::DepthLoadOp op) { depth_load_op_ = op; }

  // Uploaded mesh inspection (null-device tests / debug).
  size_t mesh_count() const { return meshes_.size(); }
  const GpuMesh* mesh_at(size_t index) const;

  // Programs created on the Device passed to record. Recreated when that
  // Device pointer changes.
  render::rhi::Pipeline* solid_pipeline() const { return solid_pipeline_; }
  render::rhi::Pipeline* textured_pipeline() const { return textured_pipeline_; }
  render::rhi::Pipeline* lit_pipeline() const { return lit_pipeline_; }

 private:
  void clear_meshes();
  void destroy_pipelines();
  bool ensure_pipelines(render::rhi::Device* device);
  // Viewport size drives world_units_per_pixel for paint-aware line/circle
  // tessellation (pixel_width / circle_radius → world units).
  bool rebuild_meshes(render::rhi::Device* device, uint32_t width,
                      uint32_t height);
  void resolve_view_envelope(uint32_t width, uint32_t height, double* min_x,
                             double* min_y, double* max_x,
                             double* max_y) const;

  uint64_t synced_generation_;
  std::vector<Instance> instances_;
  render::rhi::Device* upload_device_;
  render::rhi::Device* pipeline_device_ = nullptr;
  render::rhi::Pipeline* solid_pipeline_ = nullptr;
  render::rhi::Pipeline* textured_pipeline_ = nullptr;
  render::rhi::Pipeline* lit_pipeline_ = nullptr;
  render::rhi::Pipeline* lit_textured_pipeline_ = nullptr;
  std::vector<GpuMesh> meshes_;
  bool meshes_dirty_;
  uint32_t upload_width_;
  uint32_t upload_height_;
  bool view_ortho_set_;
  double view_min_x_;
  double view_min_y_;
  double view_max_x_;
  double view_max_y_;
  bool view_camera_set_;
  render::rhi::CameraMatrices view_camera_;
  float solid_r_;
  float solid_g_;
  float solid_b_;
  float solid_a_;
  // Terrain upload / solid gate / kTerrain record (composes TerrainPass).
  TerrainPass terrain_;
  bool background_set_;
  float background_r_;
  float background_g_;
  float background_b_;
  float background_a_;
  render::rhi::ColorLoadOp color_load_op_ = render::rhi::ColorLoadOp::kClear;
  bool enable_depth_ = false;
  render::rhi::DepthLoadOp depth_load_op_ = render::rhi::DepthLoadOp::kClear;
  bool wireframe_ = false;
  render::programs::Light light_{};
  vista::TilesetContentCache* tileset_content_cache_ = nullptr;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_PASS_H_
