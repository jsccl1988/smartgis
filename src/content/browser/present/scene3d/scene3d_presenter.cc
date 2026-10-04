// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scene3d_presenter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/host/scenic_scene_bind.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/map_contents.h"
#include "base/trace/event/process_trace.h"

#include <cstdlib>

namespace content {

Scene3dPresenter::Scene3dPresenter() {
  if (const char* env = std::getenv("SMT_SCENE3D_WIREFRAME")) {
    if (env[0] == '1' && env[1] == '\0') {
      gpu_.set_wireframe_enabled(true);
    }
  }
  atmosphere_.bind_gpu(&gpu_);
  rebind_software();
  ensure_scenic();
}

bool Scene3dPresenter::hosts_scenic_present() const {
  std::lock_guard<std::mutex> lock(scenic_mu_);
  ensure_scenic();
  return scenic_ != nullptr;
}

void Scene3dPresenter::ensure_scenic() const {
  // Drop a sticky scenic host when the selected engine is no longer scenic
  // (matrix / menu can flip SMT_SCENE3D_ENGINE after construction).
  if (!prefer_scene3d_scenic()) {
    if (scenic_) {
      scenic_->shutdown();
      scenic_.reset();
      scenic_xy_.clear();
      scenic_items_.clear();
      gpu_.render_engine_name = "pending";
    }
    return;
  }
  if (!scenic_) {
    scenic_.reset(scenic::create_scene3d_engine());
    gpu_.render_engine_name = "scenic";
  }
}

void Scene3dPresenter::set_look_preset(Scene3dLookPreset preset) {
  gpu_.set_look_preset(preset);
}

Scene3dPresenter::~Scene3dPresenter() {
  if (scenic_) {
    scenic_->shutdown();
  }
  software_.release_engine_logo_overlay();
  atmosphere_.release_passes();
  gpu_.abandon(&atmosphere_);
}

void Scene3dPresenter::rebind_software() {
  software_.bind(&gpu_, &atmosphere_, gpu_.orbit(), atmosphere_.scene(),
                 label_frame_);
  software_.set_hosts_shared_scene(hosts_shared_scene());
}

void Scene3dPresenter::bind_orbit(const OrbitFrame* orbit) {
  gpu_.bind_orbit(orbit);
  rebind_software();
}

void Scene3dPresenter::bind_label_frame(const ViewFrame* frame) {
  label_frame_ = frame;
  rebind_software();
}

void Scene3dPresenter::bind_map(const MapScene* scene) {
  map_scene_ = scene;
  atmosphere_.bind_scene(scene);
  gpu_.bind_map(scene);
  rebind_software();
}

void Scene3dPresenter::bind_contents(MapContents* session, uint32_t view_id) {
  contents_ = session;
  view_id_ = view_id;
  software_.set_hosts_shared_scene(hosts_shared_scene());
}

bool Scene3dPresenter::hosts_shared_scene() const {
  return contents_ != nullptr && view_id_ != 0;
}

Extent2 Scene3dPresenter::world_extent() const {
  return gpu_.world_extent();
}

void Scene3dPresenter::abandon_mesh() {
  gpu_.abandon(&atmosphere_);
}

void Scene3dPresenter::set_overlay_pointcloud(const float* xyz_lon_lat_elev,
                                             int point_count,
                                             const uint8_t* rgba) {
  gpu_.set_overlay_pointcloud(xyz_lon_lat_elev, point_count, rgba);
}

void Scene3dPresenter::clear_overlay_pointcloud() {
  gpu_.clear_overlay_pointcloud();
}

void Scene3dPresenter::set_overlay_tin_mesh(const float* xyz_lon_lat_elev,
                                           int point_count,
                                           const unsigned* indices,
                                           int index_count,
                                           const uint8_t* albedo_rgba) {
  gpu_.set_overlay_tin_mesh(xyz_lon_lat_elev, point_count, indices,
                            index_count, albedo_rgba);
}

void Scene3dPresenter::clear_overlay_tin_mesh() {
  gpu_.clear_overlay_tin_mesh();
}

void Scene3dPresenter::reset() {
  if (gpu_.orbit()) {
    const_cast<OrbitFrame*>(gpu_.orbit())->reset();
  }
}

void Scene3dPresenter::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kKey) {
    uint32_t k = draft.key;
    if (k >= 'a' && k <= 'z') {
      k = k - ('a' - 'A');
    }
    if (k == 'K') {
      gpu_.set_wireframe_enabled(!gpu_.wireframe_enabled());
      return;
    }
    if (k == 'J') {
      gpu_.set_wireframe_enabled(false);
      return;
    }
  }
  if (gpu_.orbit()) {
    const_cast<OrbitFrame*>(gpu_.orbit())->apply_draft(draft);
  }
}

render::rhi::CameraMatrices Scene3dPresenter::camera_matrices(
    float aspect) const {
  return gpu_.camera_matrices(aspect);
}

render::rhi::CameraMatrices Scene3dPresenter::camera_matrices_ortho(
    float width_px, float height_px) const {
  return gpu_.camera_matrices_ortho(width_px, height_px);
}

void Scene3dPresenter::set_render_engine_name(const char* name) const {
  gpu_.render_engine_name = (name && name[0]) ? name : "unknown";
}

const char* Scene3dPresenter::render_engine_name() const {
  return gpu_.render_engine_name;
}

void Scene3dPresenter::sync_scenic(uint32_t width_px, uint32_t height_px) const {
  ensure_scenic();
  if (!scenic_) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  scenic_->initialize(desc);
  scenic_->bind_orbit(
      detail::scenic_orbit_from_host(gpu_.orbit(), map_scene_));
  // Bind MapScene land rings so scenic MemFrame can draw DEM-like land /
  // coast content (still HWND-free; no FlyCube).
  const double scale = label_frame_ ? label_frame_->scale() : 1.0;
  detail::fill_scenic_draw_items(map_scene_, scale, &scenic_xy_,
                                 &scenic_items_);
  scenic_->bind_draw_items(scenic_items_.data(),
                           static_cast<uint32_t>(scenic_items_.size()));
}

bool Scene3dPresenter::present_gpu(render::rhi::Device* device,
                                  uint32_t width_px, uint32_t height_px,
                                  const ui::gfx::ShellRaster* shell,
                                  uint64_t shell_generation) {
  BASE_TRACE_EVENT("presenter", "scene3d");
  // Overlay TIN (stormsurge water / hex shell / mine clay) lives on gpu_.
  // Scenic present skips that buffer — fall through when albedo is set.
  {
    std::lock_guard<std::mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_ && !gpu_.overlay_tin_has_albedo()) {
      (void)device;
      (void)shell;
      (void)shell_generation;
      sync_scenic(width_px, height_px);
      return scenic_->present();
    }
  }
  return gpu_.present(device, width_px, height_px, shell, shell_generation,
                      atmosphere_);
}

void Scene3dPresenter::paint(HDC hdc, int width_px, int height_px,
                            bool fill_background) const {
  BASE_TRACE_EVENT("presenter_paint", "scene3d");
  // Same overlay-TIN rule as present_gpu: software_ draws free-surface /
  // hex albedo; scenic GDI proof does not.
  {
    std::lock_guard<std::mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_ && !gpu_.overlay_tin_has_albedo()) {
      (void)fill_background;
      sync_scenic(static_cast<uint32_t>(width_px),
                  static_cast<uint32_t>(height_px));
      scenic_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                         static_cast<uint32_t>(height_px));
      return;
    }
  }
  software_.paint(hdc, width_px, height_px, fill_background);
}

bool Scene3dPresenter::export_bmp(const std::string& path, int width_px,
                                  int height_px) const {
  std::lock_guard<std::mutex> lock(scenic_mu_);
  ensure_scenic();
  if (!scenic_ || path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  sync_scenic(static_cast<uint32_t>(width_px), static_cast<uint32_t>(height_px));
  return scenic_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                             static_cast<uint32_t>(height_px));
}

void Scene3dPresenter::paint_hud(HDC hdc, int width_px, int height_px) const {
  BASE_TRACE_EVENT("hud", "scene3d");
  if (!scenic_) {
    software_.paint_hud(hdc, width_px, height_px);
  }
}

void Scene3dPresenter::paint_legacy_place_labels(HDC hdc, int width_px,
                                                int height_px) const {
  if (!scenic_) {
    software_.paint_legacy_place_labels(hdc, width_px, height_px);
  }
}

}  // namespace content
