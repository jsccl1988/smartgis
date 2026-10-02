// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/scene/scene.h"

#include "effect/scene/detail/paint.h"
#include "render/programs/programs.h"

namespace effect {
namespace scene {

GpuScene::GpuScene()
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
      solid_r_(0.f),
      solid_g_(1.f),
      solid_b_(1.f),
      solid_a_(1.f),
      background_set_(false),
      background_r_(0.f),
      background_g_(0.2f),
      background_b_(0.4f),
      background_a_(1.f) {}

GpuScene::~GpuScene() {
  // Prefer abandon: callers often destroy Device before GpuScene (stack order).
  abandon();
}

void GpuScene::destroy_pipelines() {
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

bool GpuScene::ensure_pipelines(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (pipeline_device_ == device && solid_pipeline_ && textured_pipeline_ &&
      lit_pipeline_ && lit_textured_pipeline_) {
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
  if (!lit_pipeline_) {
    lit_pipeline_ =
        device->create_graphics_pipeline(render::programs::lit_pipeline_desc());
  }
  if (!lit_textured_pipeline_) {
    lit_textured_pipeline_ = device->create_graphics_pipeline(
        render::programs::lit_textured_pipeline_desc());
  }
  if (!(solid_pipeline_ && textured_pipeline_ && lit_pipeline_ &&
        lit_textured_pipeline_)) {
    destroy_pipelines();
    return false;
  }
  return true;
}

void GpuScene::release() {
  destroy_pipelines();
  clear_meshes();
}

void GpuScene::abandon() {
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

bool GpuScene::ensure_meshes(render::rhi::Device* device, uint32_t width,
                             uint32_t height) {
  if (!device || width == 0 || height == 0) {
    return false;
  }
  if (!needs_mesh_upload(device, width, height)) {
    return true;
  }
  return rebuild_meshes(device, width, height);
}

bool GpuScene::needs_mesh_upload(render::rhi::Device* device, uint32_t width,
                                uint32_t height) const {
  if (!device || width == 0 || height == 0) {
    return true;
  }
  return meshes_dirty_ || upload_device_ != device || upload_width_ != width ||
         upload_height_ != height;
}

void GpuScene::set_view_ortho(double min_x, double min_y, double max_x,
                              double max_y) {
  view_min_x_ = min_x;
  view_min_y_ = min_y;
  view_max_x_ = max_x;
  view_max_y_ = max_y;
  view_ortho_set_ = true;
  meshes_dirty_ = true;
}

void GpuScene::clear_view_ortho() {
  view_ortho_set_ = false;
  meshes_dirty_ = true;
}

void GpuScene::set_view_camera(const render::rhi::CameraMatrices& camera) {
  view_camera_ = camera;
  view_camera_set_ = true;
}

void GpuScene::clear_view_camera() {
  view_camera_set_ = false;
}

void GpuScene::set_solid_color(float r, float g, float b, float a) {
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
    // Per-instance paint owns tint; leave those meshes alone.
    if (i < instances_.size() && instances_[i].has_paint) {
      continue;
    }
    meshes_[i].solid_r = r;
    meshes_[i].solid_g = g;
    meshes_[i].solid_b = b;
    meshes_[i].solid_a = a;
  }
}

void GpuScene::set_solid_color_from_colorref(long colorref) {
  const float r = static_cast<float>((colorref >> 0) & 0xff) / 255.f;
  const float g = static_cast<float>((colorref >> 8) & 0xff) / 255.f;
  const float b = static_cast<float>((colorref >> 16) & 0xff) / 255.f;
  set_solid_color(r, g, b, 1.f);
}

bool GpuScene::set_instance_paint(size_t index,
                                 const gis::style::ResolvedPaint& paint) {
  if (index >= instances_.size()) {
    return false;
  }
  instances_[index].has_paint = true;
  instances_[index].paint = paint;
  meshes_dirty_ = true;
  return true;
}

void GpuScene::clear_instance_paint(size_t index) {
  if (index >= instances_.size()) {
    return;
  }
  instances_[index].has_paint = false;
  instances_[index].paint = gis::style::ResolvedPaint();
  meshes_dirty_ = true;
}

void GpuScene::set_background_paint(const gis::style::ResolvedPaint& paint) {
  // Background layers use fill_color / fill_opacity (style maps
  // background-color into fill_* on ResolvedPaint when present).
  detail::argb_to_rgba(paint.fill_color, paint.fill_opacity, &background_r_,
               &background_g_, &background_b_, &background_a_);
  background_set_ = true;
}

void GpuScene::clear_background_paint() {
  background_set_ = false;
  background_r_ = 0.f;
  background_g_ = 0.2f;
  background_b_ = 0.4f;
  background_a_ = 1.f;
}

const GpuScene::GpuMesh* GpuScene::mesh_at(size_t index) const {
  if (index >= meshes_.size()) {
    return nullptr;
  }
  return &meshes_[index];
}

const GpuInstance* GpuScene::instance_at(size_t index) const {
  if (index >= instances_.size()) {
    return nullptr;
  }
  return &instances_[index];
}

void GpuScene::sync_from(const gis::World& world) {
  // Empty GPU instances with live World nodes means a prior abandon/clear left
  // synced_generation_ matching world without a follow-up copy (or a stale
  // DLL skipped the post-abandon resync). Force a rebuild in that case.
  if (world.generation() == synced_generation_ &&
      !(instances_.empty() && world.node_count() > 0)) {
    return;
  }
  // Build into a fresh vector then swap — avoids Debug STL orphan-proxy AV when
  // instances_ was cleared/reused after abandon across Device boundaries.
  std::vector<GpuInstance> next;
  const size_t n = world.node_count();
  next.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const gis::Node* node = world.node_at(i);
    if (!node) {
      continue;
    }
    GpuInstance inst;
    inst.node_id = node->id;
    inst.kind = node->kind;
    inst.min_x = node->min_x;
    inst.min_y = node->min_y;
    inst.min_z = node->min_z;
    inst.max_x = node->max_x;
    inst.max_y = node->max_y;
    inst.max_z = node->max_z;
    inst.layer = node->layer;
    inst.ogr_layer = node->ogr_layer;
    inst.geom_3d = node->geom_3d;
    inst.geoms = node->geoms;
    inst.tin = node->tin;
    inst.grid = node->grid;
    inst.model = node->model;
    inst.tileset = node->tileset;
    inst.visible_uris = node->visible_uris;
    inst.terrain_positions = node->terrain_positions;
    inst.terrain_indices = node->terrain_indices;
    inst.terrain_uvs = node->terrain_uvs;
    inst.terrain_rgba = node->terrain_rgba;
    inst.terrain_tex_w = node->terrain_tex_w;
    inst.terrain_tex_h = node->terrain_tex_h;
    inst.point_positions = node->point_positions;
    inst.point_rgba = node->point_rgba;
    inst.point_chunks = node->point_chunks;
    inst.has_paint = false;
    next.push_back(std::move(inst));
  }
  instances_.swap(next);
  synced_generation_ = world.generation();
  meshes_dirty_ = true;
}

}  // namespace scene
}  // namespace effect
