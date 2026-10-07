// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GN-DEP: //src/scenic:scenic

#include "content/browser/present/scene3d/scenic_engine_host.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/host/scenic_scene_bind.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"

namespace content {
namespace detail {

ScenicScene3dHost::ScenicScene3dHost() = default;

ScenicScene3dHost::~ScenicScene3dHost() {
  shutdown();
}

bool ScenicScene3dHost::is_live() const {
  std::lock_guard<std::mutex> lock(mu_);
  return engine_ != nullptr;
}

bool ScenicScene3dHost::ensure_locked() {
  // Product SoT is Vista WorldPass — never construct scenic on that path.
  if (!prefer_scene3d_scenic()) {
    if (engine_) {
      engine_->shutdown();
      engine_.reset();
      xy_.clear();
      items_.clear();
      sync_valid_ = false;
    }
    return false;
  }
  if (!engine_) {
    // set_scene3d_engine(kScenic) loads the façade. Retry here so a
    // failed first load can succeed before create_scene3d_engine.
    if (!load_scene3d_scenic_dll()) {
      return false;
    }
    engine_.reset(scenic::create_scene3d_engine());
    sync_valid_ = false;
  }
  return engine_ != nullptr;
}

bool ScenicScene3dHost::ensure() {
  std::lock_guard<std::mutex> lock(mu_);
  return ensure_locked();
}

bool ScenicScene3dHost::sync_inputs_unchanged(uint32_t width_px,
                                              uint32_t height_px,
                                              const OrbitFrame* orbit,
                                              const GisScene* scene,
                                              const ViewFrame* labels) const {
  if (!sync_valid_ || !engine_ || items_.empty()) {
    return false;
  }
  if (sync_w_ != width_px || sync_h_ != height_px || sync_orbit_ != orbit ||
      sync_scene_ != scene || sync_labels_ != labels) {
    return false;
  }
  const float yaw = orbit ? orbit->yaw() : 0.f;
  const float pitch = orbit ? orbit->pitch() : 0.f;
  const float distance = orbit ? orbit->distance() : 0.f;
  const double scale = labels ? labels->scale() : 1.0;
  return sync_yaw_ == yaw && sync_pitch_ == pitch &&
         sync_distance_ == distance && sync_label_scale_ == scale;
}

void ScenicScene3dHost::remember_sync_inputs(uint32_t width_px,
                                             uint32_t height_px,
                                             const OrbitFrame* orbit,
                                             const GisScene* scene,
                                             const ViewFrame* labels) {
  sync_w_ = width_px;
  sync_h_ = height_px;
  sync_orbit_ = orbit;
  sync_yaw_ = orbit ? orbit->yaw() : 0.f;
  sync_pitch_ = orbit ? orbit->pitch() : 0.f;
  sync_distance_ = orbit ? orbit->distance() : 0.f;
  sync_scene_ = scene;
  sync_labels_ = labels;
  sync_label_scale_ = labels ? labels->scale() : 1.0;
  sync_valid_ = true;
}

void ScenicScene3dHost::sync_locked(uint32_t width_px, uint32_t height_px,
                                    const OrbitFrame* orbit,
                                    const GisScene* scene,
                                    const ViewFrame* labels) {
  if (!engine_) {
    return;
  }
  // Warm scenic frames: skip fill_scenic_draw_items + rebind when inputs match.
  if (sync_inputs_unchanged(width_px, height_px, orbit, scene, labels)) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  engine_->initialize(desc);
  engine_->bind_orbit(scenic_orbit_from_host(orbit, scene));
  const double scale = labels ? labels->scale() : 1.0;
  fill_scenic_draw_items(scene, scale, &xy_, &items_);
  engine_->bind_draw_items(items_.data(),
                           static_cast<uint32_t>(items_.size()));
  remember_sync_inputs(width_px, height_px, orbit, scene, labels);
}

bool ScenicScene3dHost::present(uint32_t width_px, uint32_t height_px,
                                const OrbitFrame* orbit, const GisScene* scene,
                                const ViewFrame* labels) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!ensure_locked()) {
    return false;
  }
  sync_locked(width_px, height_px, orbit, scene, labels);
  return engine_->present();
}

bool ScenicScene3dHost::paint_hdc(HDC hdc, int width_px, int height_px,
                                  const OrbitFrame* orbit,
                                  const GisScene* scene,
                                  const ViewFrame* labels) {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!ensure_locked()) {
    return false;
  }
  sync_locked(static_cast<uint32_t>(width_px),
              static_cast<uint32_t>(height_px), orbit, scene, labels);
  engine_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                     static_cast<uint32_t>(height_px));
  return true;
}

bool ScenicScene3dHost::export_bmp(const std::string& path, int width_px,
                                   int height_px, const OrbitFrame* orbit,
                                   const GisScene* scene,
                                   const ViewFrame* labels) {
  if (path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (!ensure_locked()) {
    return false;
  }
  sync_locked(static_cast<uint32_t>(width_px),
              static_cast<uint32_t>(height_px), orbit, scene, labels);
  return engine_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                             static_cast<uint32_t>(height_px));
}

void ScenicScene3dHost::shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  if (engine_) {
    engine_->shutdown();
    engine_.reset();
  }
  xy_.clear();
  items_.clear();
  sync_valid_ = false;
}

}  // namespace detail
}  // namespace content
