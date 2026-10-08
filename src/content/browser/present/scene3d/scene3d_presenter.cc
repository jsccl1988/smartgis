// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scene3d_presenter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/gis_contents.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"

#include <cstdlib>

namespace content {

void Scene3dPresenter::Deleter::operator()(Scene3dPresenter* p) const {
  delete p;
}

Scene3dPresenter::Ptr Scene3dPresenter::create() {
  return Ptr(new Scene3dPresenter);
}

Scene3dPresenter::Scene3dPresenter() {
  atmosphere_ = AtmosphereSession::create();
  if (const char* env = base::switch_cstr("scene3d-wireframe")) {
    if (env[0] == '1' && env[1] == '\0') {
      gpu_.set_wireframe_enabled(true);
    }
  }
  atmosphere_->bind_gpu(&gpu_);
  rebind_hdc();
  if (scenic_host_.ensure()) {
    gpu_.render_engine_name = "scenic";
  }
}

bool Scene3dPresenter::hosts_scenic_present() const {
  return scenic_host_.ensure();
}

void Scene3dPresenter::set_look_preset(Scene3dLookPreset preset) {
  gpu_.set_look_preset(preset);
}

Scene3dLookPreset Scene3dPresenter::look_preset() const {
  return gpu_.look_preset();
}

bool Scene3dPresenter::ensure_legacy_overlays() {
  return gpu_.ensure_legacy_overlays();
}

int Scene3dPresenter::legacy_label_count() const {
  return gpu_.legacy_label_count();
}

bool Scene3dPresenter::has_legacy_coast_vectors() const {
  return gpu_.has_legacy_coast_vectors();
}

Scene3dPresenter::~Scene3dPresenter() {
  hdc_.release_engine_logo_overlay();
  if (atmosphere_) {
    atmosphere_->release_passes();
    gpu_.abandon(atmosphere_.get());
  }
}

void Scene3dPresenter::rebind_hdc() {
  hdc_.bind(&gpu_, atmosphere_.get(), gpu_.orbit(),
                 atmosphere_ ? atmosphere_->scene() : nullptr, label_frame_);
  hdc_.set_hosts_shared_scene(hosts_shared_scene());
}

void Scene3dPresenter::bind_orbit(const OrbitFrame* orbit) {
  gpu_.bind_orbit(orbit);
  rebind_hdc();
}

void Scene3dPresenter::bind_label_frame(const ViewFrame* frame) {
  label_frame_ = frame;
  rebind_hdc();
}

void Scene3dPresenter::bind_scene(const GisScene* scene) {
  gis_scene_ = scene;
  if (atmosphere_) {
    atmosphere_->bind_scene(scene);
  }
  gpu_.bind_scene(scene);
  rebind_hdc();
}

void Scene3dPresenter::bind_contents(GisContents* session, uint32_t view_id) {
  contents_ = session;
  view_id_ = view_id;
  hdc_.set_hosts_shared_scene(hosts_shared_scene());
}

bool Scene3dPresenter::hosts_shared_scene() const {
  return contents_ != nullptr && view_id_ != 0;
}

Extent2 Scene3dPresenter::world_extent() const {
  return gpu_.world_extent();
}

void Scene3dPresenter::abandon_mesh() {
  gpu_.abandon(atmosphere_.get());
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

void Scene3dPresenter::set_overlay_tin_drape(const uint8_t* rgba, uint32_t width,
                                            uint32_t height, const float* uv,
                                            int uv_float_count) {
  gpu_.set_overlay_tin_drape(rgba, width, height, uv, uv_float_count);
}

void Scene3dPresenter::clear_overlay_tin_mesh() {
  gpu_.clear_overlay_tin_mesh();
}

void Scene3dPresenter::set_dem_drape_rgba(const uint8_t* rgba, uint32_t width,
                                         uint32_t height) {
  gpu_.set_dem_drape_rgba(rgba, width, height);
}

void Scene3dPresenter::clear_dem_drape() {
  gpu_.clear_dem_drape();
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

bool Scene3dPresenter::present_gpu(render::rhi::Device* device,
                                  uint32_t width_px, uint32_t height_px,
                                  const ui::gfx::ShellRaster* shell,
                                  uint64_t shell_generation) {
  BASE_TRACE_EVENT("presenter", "scene3d");
  // Overlay TIN (stormsurge water / hex shell / mine clay) lives on gpu_.
  // Scenic present skips that buffer — fall through when albedo is set.
  const bool scenic_live = scenic_host_.ensure();
  if (scenic_live && !gpu_.overlay_tin_has_albedo()) {
    gpu_.render_engine_name = "scenic";
    (void)device;
    (void)shell;
    (void)shell_generation;
    return scenic_host_.present(width_px, height_px, gpu_.orbit(), gis_scene_,
                                label_frame_);
  }
  return gpu_.present(device, width_px, height_px, shell, shell_generation,
                      *atmosphere_);
}

void Scene3dPresenter::paint(HDC hdc, int width_px, int height_px,
                            bool fill_background) const {
  BASE_TRACE_EVENT("presenter_paint", "scene3d");
  // Product DrawHost skips GPU present when --scene3d-engine=scenic, so the
  // HWND path is the one that must present the hosted engine. Software DEM
  // remains the fallback when scenic.dll did not load.
  if (scenic_host_.paint_hdc(hdc, width_px, height_px, gpu_.orbit(),
                             gis_scene_, label_frame_)) {
    gpu_.render_engine_name = "scenic";
    return;
  }
  if (prefer_scene3d_scenic()) {
    gpu_.render_engine_name = "scenic";
  }
  hdc_.paint(hdc, width_px, height_px, fill_background);
}

bool Scene3dPresenter::export_bmp(const std::string& path, int width_px,
                                  int height_px) const {
  return scenic_host_.export_bmp(path, width_px, height_px, gpu_.orbit(),
                                 gis_scene_, label_frame_);
}

void Scene3dPresenter::paint_hud(HDC hdc, int width_px, int height_px) const {
  BASE_TRACE_EVENT("hud", "scene3d");
  if (!scenic_host_.is_live()) {
    hdc_.paint_hud(hdc, width_px, height_px);
  }
}

void Scene3dPresenter::paint_legacy_place_labels(HDC hdc, int width_px,
                                                int height_px) const {
  if (!scenic_host_.is_live()) {
    hdc_.paint_legacy_place_labels(hdc, width_px, height_px);
  }
}

}  // namespace content
