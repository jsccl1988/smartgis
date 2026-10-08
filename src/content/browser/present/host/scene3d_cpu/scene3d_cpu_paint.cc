// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/hdc/scene3d_hdc_painter.h"

#include "content/browser/present/host/gdi/gdi_primitives.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"
#include "content/browser/present/scene3d/hdc/scene3d_hdc_hud.h"
#include "content/browser/present/scene3d/hdc/scene3d_hdc_logo.h"
#include "content/browser/present/scene3d/hdc/scene3d_hdc_mesh.h"
#include "base/trace/event/process_trace.h"

#include <cstring>
#include <mutex>

namespace content {

Scene3dHdcPainter::~Scene3dHdcPainter() {
  release_engine_logo_overlay();
  if (place_label_font_) {
    DeleteObject(place_label_font_);
    place_label_font_ = nullptr;
  }
}

void Scene3dHdcPainter::bind(Scene3dGpuPresent* gpu,
                                  AtmosphereSession* atmosphere,
                                  const OrbitFrame* orbit,
                                  const GisScene* scene,
                                  const ViewFrame* label_frame) {
  gpu_ = gpu;
  atmosphere_ = atmosphere;
  orbit_ = orbit;
  scene_ = scene;
  label_frame_ = label_frame;
}

void Scene3dHdcPainter::paint_engine_logo(HDC hdc, int width_px,
                                               int height_px,
                                               const char* engine_name) {
  detail::paint_engine_logo_corner(hdc, width_px, height_px, engine_name);
}

void Scene3dHdcPainter::release_engine_logo_overlay() const {
  detail::release_engine_logo_overlay(&logo_hwnd_);
}

void Scene3dHdcPainter::paint_legacy_place_labels(HDC hdc, int width_px,
                                                       int height_px) const {
  detail::paint_soft_legacy_place_labels(hdc, width_px, height_px, gpu_,
                                         orbit_, &place_label_font_);
}

void Scene3dHdcPainter::paint_hud(HDC hdc, int width_px,
                                       int height_px) const {
  if (!hdc || !gpu_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  gpu_->remember_view_size(width_px, height_px);
  // Place-names before compass/horizon so leftover stereo labels sit on DEM.
  detail::paint_soft_legacy_place_labels(hdc, width_px, height_px, gpu_,
                                         orbit_, &place_label_font_);
  detail::paint_soft_wind_arrows(hdc, width_px, height_px, atmosphere_, gpu_,
                                 orbit_);
  detail::paint_soft_hud_status(hdc, width_px, height_px, gpu_, atmosphere_,
                                hosts_shared_scene_);
  if (gpu_->wireframe_enabled()) {
    detail::paint_soft_wireframe_edges(hdc, width_px, height_px, gpu_, orbit_);
  }
  // Engine badge last so it stays readable over wireframe.
  detail::paint_soft_hud_engine_badge(hdc, width_px, height_px, gpu_,
                                      &logo_hwnd_);
}

void Scene3dHdcPainter::paint(HDC hdc, int width_px, int height_px,
                                   bool fill_background) const {
  BASE_TRACE_EVENT("gdi", "scene3d.gdi");
  if (!hdc || !gpu_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(gpu_->mutex());
  gpu_->remember_view_size(width_px, height_px);
  // Soft paint is the DEM SoT path, but export_scene3d_bmp may prove FlyCube
  // GpuPresent then soft-export the shared mesh. Do not wipe a caller-set GPU
  // engine label (sidecar + HUD must agree; visual_review #1).
  {
    const char* prior = gpu_->render_engine_name;
    const bool keep_gpu_label =
        prior && prior[0] &&
        (std::strncmp(prior, "FlyCube/", 8) == 0 ||
         std::strcmp(prior, "scenic") == 0 ||
         std::strncmp(prior, "Stereo/", 7) == 0);
    if (!keep_gpu_label) {
      gpu_->render_engine_name = "GDI";
    }
  }
  gpu_->note_present_frame();

  if (fill_background) {
    // Leftover stereo: black void. Atmosphere product face: soft sky clear so
    // DEM hypsometric fills are not framed by a hollow black band.
    const bool legacy =
        gpu_->look_preset() == Scene3dLookPreset::kLegacyStereo;
    detail::gdi_fill_rect(hdc, 0, 0, width_px, height_px,
                          legacy ? RGB(0, 0, 0) : RGB(120, 165, 210));
  }

  gpu_->rebuild_local_mesh();
  gpu_->attach_overlay_tin_locked();
  gpu_->attach_overlay_pointcloud_locked();

  detail::paint_soft_scene_body(hdc, width_px, height_px, gpu_, atmosphere_,
                                orbit_, &mesh_prep_);

  // HUD paints leftover place-names once (avoid double TextOut outline).
  paint_hud(hdc, width_px, height_px);
}

}  // namespace content
