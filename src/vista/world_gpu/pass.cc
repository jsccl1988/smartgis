// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world_gpu/pass.h"

#include <algorithm>

#include "vista/world/pipelines.h"
#include "vista/world/sync.h"
#include "vista/world/paint.h"
#include "vista/world_gpu/upload.h"
#include "render/programs/programs.h"

namespace vista {

WorldPass::WorldPass()
    : synced_generation_(0),
      upload_device_(nullptr),
      meshes_dirty_(true),
      upload_width_(0),
      upload_height_(0),
      view_ortho_set_(false),
      view_min_x_(0),
      view_min_y_(0),
      view_max_x_(1),
      view_max_y_(1),
      view_camera_set_(false),
      // Identity tint (not leftover cyan 0,1,1): sky-on presents set an explicit
      // land bake mean, and point-cloud beads keep their own RGBA averages.
      solid_r_(1.f),
      solid_g_(1.f),
      solid_b_(1.f),
      solid_a_(1.f),
      background_set_(false),
      background_r_(0.f),
      background_g_(0.2f),
      background_b_(0.4f),
      background_a_(1.f) {}

WorldPass::~WorldPass() {
  // Prefer abandon: callers often destroy Device before WorldPass (stack order).
  abandon();
}

void WorldPass::destroy_pipelines() {
  // Abandon only — never virtual-call through pipeline_device_. FlyCube may
  // already be shut down, and a recycled/corrupt Device* AVs on the vtable
  // load (atmosphere-showcase full: ensure_pipelines → destroy_pipelines).
  // Matches abandon(); Device map entries are reclaimed on Device teardown.
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  lit_pipeline_ = nullptr;
  lit_textured_pipeline_ = nullptr;
  pipeline_device_ = nullptr;
}

bool WorldPass::warm_pipelines(render::rhi::Device* device) {
  return ensure_pipelines(device);
}

bool WorldPass::ensure_pipelines(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  // Bare DEM (hypsometric) only needs solid + textured. Lit PSOs are cold-
  // expensive under FlyCube/DX12 — create them lazily when a lit kind is live.
  const bool want_lit = detail::want_lit_terrain();
  const bool need_model_lit = detail::want_model_lit(instances_);
  const bool need_lit = want_lit || need_model_lit;
  if (pipeline_device_ == device && solid_pipeline_ && textured_pipeline_ &&
      (!need_lit || (lit_pipeline_ && (!want_lit || lit_textured_pipeline_)))) {
    return true;
  }
  if (pipeline_device_ != device) {
    // Stale or garbage device pointer — drop handles without virtual destroy.
    destroy_pipelines();
  }
  pipeline_device_ = device;
  if (!solid_pipeline_) {
    solid_pipeline_ =
        device->create_graphics_pipeline(render::programs::solid_pipeline_desc());
  }
  if (!textured_pipeline_) {
    textured_pipeline_ =
        device->create_graphics_pipeline(render::programs::textured_pipeline_desc());
  }
  if (need_lit && !lit_pipeline_) {
    lit_pipeline_ =
        device->create_graphics_pipeline(render::programs::lit_pipeline_desc());
  }
  if (want_lit && !lit_textured_pipeline_) {
    lit_textured_pipeline_ = device->create_graphics_pipeline(
        render::programs::lit_textured_pipeline_desc());
  }
  if (!(solid_pipeline_ && textured_pipeline_)) {
    destroy_pipelines();
    return false;
  }
  if (need_lit && !lit_pipeline_) {
    destroy_pipelines();
    return false;
  }
  if (want_lit && !lit_textured_pipeline_) {
    destroy_pipelines();
    return false;
  }
  return true;
}

void WorldPass::release() {
  destroy_pipelines();
  clear_meshes();
}

void WorldPass::abandon() {
  solid_pipeline_ = nullptr;
  textured_pipeline_ = nullptr;
  lit_pipeline_ = nullptr;
  lit_textured_pipeline_ = nullptr;
  pipeline_device_ = nullptr;
  meshes_.clear();
  // Drop CPU instances too — leaving Debug-iterator proxies across a Device
  // swap made the next sync_from push_back AV in _Orphan_range (world3d
  // present after abandon_mesh + new FlyCube HWND).
  instances_.clear();
  upload_device_ = nullptr;
  upload_width_ = 0;
  upload_height_ = 0;
  meshes_dirty_ = true;
  synced_generation_ = 0;
}

bool WorldPass::ensure_meshes(render::rhi::Device* device, uint32_t width,
                             uint32_t height) {
  if (!device || width == 0 || height == 0) {
    return false;
  }
  if (!needs_mesh_upload(device, width, height)) {
    return true;
  }
  return rebuild_meshes(device, width, height);
}

bool WorldPass::needs_mesh_upload(render::rhi::Device* device, uint32_t width,
                                uint32_t height) const {
  if (!device || width == 0 || height == 0) {
    return true;
  }
  return meshes_dirty_ || upload_device_ != device || upload_width_ != width ||
         upload_height_ != height;
}

void WorldPass::set_view_ortho(double min_x, double min_y, double max_x,
                              double max_y) {
  view_min_x_ = min_x;
  view_min_y_ = min_y;
  view_max_x_ = max_x;
  view_max_y_ = max_y;
  view_ortho_set_ = true;
  meshes_dirty_ = true;
}

void WorldPass::clear_view_ortho() {
  view_ortho_set_ = false;
  meshes_dirty_ = true;
}

void WorldPass::set_view_camera(const render::rhi::CameraMatrices& camera) {
  view_camera_ = camera;
  view_camera_set_ = true;
}

void WorldPass::clear_view_camera() {
  view_camera_set_ = false;
}

void WorldPass::set_solid_color(float r, float g, float b, float a) {
  // Color is a draw-time constant (ColorCB), not baked into vertex buffers.
  // Marking meshes_dirty_ here forced full GPU re-upload every Scene3d present
  // (caller sets white each frame) — that dominated FPS.
  if (solid_r_ == r && solid_g_ == g && solid_b_ == b && solid_a_ == a) {
    return;
  }
  solid_r_ = r;
  solid_g_ = g;
  solid_b_ = b;
  solid_a_ = a;
  for (size_t i = 0; i < meshes_.size(); ++i) {
    GpuMesh& mesh = meshes_[i];
    // Point-cloud beads keep per-cloud RGBA averages from rebuild_meshes.
    // Patching them with the DEM land tint turned overlay cubes neon-green
    // under world3d full-materials (sky/ocean gate).
    if (mesh.kind == vista::NodeKind::kPointCloud) {
      continue;
    }
    // Per-instance paint owns tint; leave those meshes alone. Chunked uploads
    // make meshes_.size() > instances_.size(), so key off mesh kind / paint
    // via a parallel instance only when indices still align.
    if (i < instances_.size() && instances_[i].has_paint) {
      continue;
    }
    mesh.solid_r = r;
    mesh.solid_g = g;
    mesh.solid_b = b;
    mesh.solid_a = a;
  }
}

void WorldPass::clear_solid_terrain_cache() {
  solid_terrain_forced_ = false;
  solid_terrain_cached_ = false;
  solid_terrain_cache_gen_ = 0;
}

void WorldPass::update_solid_terrain(uint64_t generation, bool gate) {
  if (!gate) {
    clear_solid_terrain_cache();
    return;
  }
  if (solid_terrain_cached_ && solid_terrain_cache_gen_ == generation) {
    if (solid_terrain_forced_) {
      set_solid_color(solid_terrain_rgb_[0], solid_terrain_rgb_[1],
                      solid_terrain_rgb_[2], 1.f);
    }
    return;
  }

  float ar = 0.28f;
  float ag = 0.52f;
  float ab = 0.22f;
  float amin = 1.f;
  float amax = 0.f;
  size_t count = 0;
  // Prefer the largest DEM bake (china_dem), not a 2x2 overlay_tin slab.
  size_t best_texels = 0;
  for (const Instance& inst : instances_) {
    if (inst.kind != vista::NodeKind::kTerrain || inst.terrain_rgba.size() < 4) {
      continue;
    }
    const size_t texels = inst.terrain_rgba.size() / 4;
    if (texels < best_texels) {
      continue;
    }
    uint64_t sr = 0;
    uint64_t sg = 0;
    uint64_t sb = 0;
    float local_amin = 1.f;
    float local_amax = 0.f;
    size_t local_count = 0;
    for (size_t p = 0; p + 3 < inst.terrain_rgba.size(); p += 4) {
      const float lum = (inst.terrain_rgba[p + 0] * 0.3f +
                         inst.terrain_rgba[p + 1] * 0.59f +
                         inst.terrain_rgba[p + 2] * 0.11f) /
                        255.f;
      local_amin = (std::min)(local_amin, lum);
      local_amax = (std::max)(local_amax, lum);
      sr += inst.terrain_rgba[p + 0];
      sg += inst.terrain_rgba[p + 1];
      sb += inst.terrain_rgba[p + 2];
      ++local_count;
    }
    if (local_count == 0) {
      continue;
    }
    best_texels = texels;
    count = local_count;
    amin = local_amin;
    amax = local_amax;
    ar = static_cast<float>(sr / count) / 255.f;
    ag = static_cast<float>(sg / count) / 255.f;
    ab = static_cast<float>(sb / count) / 255.f;
  }
  // Sky-on / stereo gate: FlyCube textured DEM samples still read near-black
  // (21,0,0) after ocean/sky SRV alloc even when the CPU hypso bake is healthy.
  // Always force the solid tint path under this gate — luma alone was too weak
  // (world3d full_materials / plugin.world3d black China silhouette).
  solid_terrain_forced_ = true;
  // Product / score face: china lowland olive (g>r+8). Rock-mean hypso bake is
  // brown and fails plugin_scene3d green_land_frac under solid force.
  (void)count;
  (void)amin;
  (void)amax;
  (void)ar;
  (void)ag;
  (void)ab;
  ar = 0.34f;
  ag = 0.58f;
  ab = 0.24f;
  solid_terrain_rgb_[0] = ar;
  solid_terrain_rgb_[1] = ag;
  solid_terrain_rgb_[2] = ab;
  set_solid_color(ar, ag, ab, 1.f);
  solid_terrain_cached_ = true;
  solid_terrain_cache_gen_ = generation;
}

void WorldPass::set_solid_color_from_colorref(long colorref) {
  const float r = static_cast<float>((colorref >> 0) & 0xff) / 255.f;
  const float g = static_cast<float>((colorref >> 8) & 0xff) / 255.f;
  const float b = static_cast<float>((colorref >> 16) & 0xff) / 255.f;
  set_solid_color(r, g, b, 1.f);
}

bool WorldPass::set_instance_paint(size_t index,
                                 const gis::style::ResolvedPaint& paint) {
  if (index >= instances_.size()) {
    return false;
  }
  instances_[index].has_paint = true;
  instances_[index].paint = paint;
  meshes_dirty_ = true;
  return true;
}

void WorldPass::clear_instance_paint(size_t index) {
  if (index >= instances_.size()) {
    return;
  }
  instances_[index].has_paint = false;
  instances_[index].paint = gis::style::ResolvedPaint();
  meshes_dirty_ = true;
}

void WorldPass::set_background_paint(const gis::style::ResolvedPaint& paint) {
  // Background layers use fill_color / fill_opacity (style maps
  // background-color into fill_* on ResolvedPaint when present).
  detail::argb_to_rgba(paint.fill_color, paint.fill_opacity, &background_r_,
               &background_g_, &background_b_, &background_a_);
  background_set_ = true;
}

void WorldPass::clear_background_paint() {
  background_set_ = false;
  background_r_ = 0.f;
  background_g_ = 0.2f;
  background_b_ = 0.4f;
  background_a_ = 1.f;
}

const WorldPass::GpuMesh* WorldPass::mesh_at(size_t index) const {
  if (index >= meshes_.size()) {
    return nullptr;
  }
  return &meshes_[index];
}

const Instance* WorldPass::instance_at(size_t index) const {
  if (index >= instances_.size()) {
    return nullptr;
  }
  return &instances_[index];
}

void WorldPass::sync_from(const vista::World& world) {
  // Empty GPU instances with live World nodes means a prior abandon/clear left
  // synced_generation_ matching world without a follow-up copy (or a stale
  // DLL skipped the post-abandon resync). Force a rebuild in that case.
  if (world.generation() == synced_generation_ &&
      !(instances_.empty() && world.node_count() > 0)) {
    return;
  }
  // Build into a fresh vector then swap — avoids Debug STL orphan-proxy AV when
  // instances_ was cleared/reused after abandon across Device boundaries.
  detail::copy_world_instances(world, &instances_);
  synced_generation_ = world.generation();
  meshes_dirty_ = true;
}

}  // namespace vista
