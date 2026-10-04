// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scenic_engine_host.h"

#include <vector>

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
  if (!prefer_scene3d_scenic()) {
    if (engine_) {
      engine_->shutdown();
      engine_.reset();
    }
    return false;
  }
  if (!engine_) {
    engine_.reset(scenic::create_scene3d_engine());
  }
  return engine_ != nullptr;
}

bool ScenicScene3dHost::ensure() {
  std::lock_guard<std::mutex> lock(mu_);
  return ensure_locked();
}

void ScenicScene3dHost::sync_locked(uint32_t width_px, uint32_t height_px,
                                    const OrbitFrame* orbit,
                                    const MapScene* scene,
                                    const ViewFrame* labels) {
  if (!engine_) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  engine_->initialize(desc);
  engine_->bind_orbit(scenic_orbit_from_host(orbit, scene));
  const double scale = labels ? labels->scale() : 1.0;
  std::vector<scenic::Vertex2> xy;
  std::vector<scenic::DrawItem> items;
  fill_scenic_draw_items(scene, scale, &xy, &items);
  engine_->bind_draw_items(items.data(),
                           static_cast<uint32_t>(items.size()));
}

bool ScenicScene3dHost::present(uint32_t width_px, uint32_t height_px,
                                const OrbitFrame* orbit, const MapScene* scene,
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
                                  const MapScene* scene,
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
                                   const MapScene* scene,
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
}

}  // namespace detail
}  // namespace content
