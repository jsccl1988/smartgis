// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/host/scenic_scene_bind.h"

#include <cstdlib>
#include <cstring>

namespace content {

bool prefer_map2d_scenic() {
  const char* raw = base::switch_cstr("map2d-engine");
  return raw && raw[0] && _stricmp(raw, "scenic") == 0;
}

void Map2dPresenter::Deleter::operator()(Map2dPresenter* p) const {
  delete p;
}

Map2dPresenter::Ptr Map2dPresenter::create() {
  return Ptr(new Map2dPresenter);
}

Map2dPresenter::Map2dPresenter() {
  ensure_scenic();
}

Map2dPresenter::~Map2dPresenter() = default;

void Map2dPresenter::ensure_scenic() const {
  // Drop sticky scenic when MAP2D_ENGINE is no longer scenic so product
  // Vista/FlyCube present is not permanently hijacked after a matrix cell.
  if (!prefer_map2d_scenic()) {
    if (scenic_) {
      scenic_->shutdown();
      scenic_.reset();
      scenic_xy_.clear();
      scenic_items_.clear();
    }
    return;
  }
  if (!scenic_) {
    scenic_.reset(scenic::create_map2d_engine());
  }
}

bool Map2dPresenter::hosts_scenic_present() const {
  // Product path never enables scenic; skip scenic_mu_ so a skewed
  // BrowserSession layout cannot unlock a non-mutex blob during WM_PAINT.
  if (!prefer_map2d_scenic()) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
  ensure_scenic();
  return scenic_ != nullptr;
}

void Map2dPresenter::bind(const GisScene* scene, const ViewFrame* frame) {
  scene_ = scene;
  frame_ = frame;
  cache_.bind(scene, frame);
  gpu_.bind(scene, frame, &cache_);
  hdc_.bind(scene, frame, &cache_);
}

void Map2dPresenter::invalidate_frame_cache() {
  // Invariant: visibility / style / feature edits change ContentFingerprint and
  // must still drop the published MapIR here. Extent / pan / fly must not —
  // prepare_for_present already keys camera separately (InteractiveReuse).
  if (!cache_.content_differs_from_cache()) {
    return;
  }
  gpu_.invalidate_frame_cache();
  hdc_.invalidate_present_cache();
}

void Map2dPresenter::sync_scenic(uint32_t width_px, uint32_t height_px) const {
  ensure_scenic();
  if (!scenic_) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  scenic_->initialize(desc);
  const double scale = frame_ ? frame_->scale() : 1.0;
  detail::fill_scenic_draw_items(scene_, scale, &scenic_xy_, &scenic_items_);
  scenic_->bind_view(detail::scenic_view_from_frame(frame_));
  scenic_->bind_draw_items(scenic_items_.data(),
                           static_cast<uint32_t>(scenic_items_.size()));
}

bool Map2dPresenter::present_gpu(render::rhi::Device* device, uint32_t width_px,
                                 uint32_t height_px,
                                 const ui::gfx::ShellRaster* shell,
                                 uint64_t shell_generation) {
  if (prefer_map2d_scenic()) {
    std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_) {
      (void)device;
      (void)shell;
      (void)shell_generation;
      sync_scenic(width_px, height_px);
      return scenic_->present();
    }
  }
  return gpu_.present(device, width_px, height_px, shell, shell_generation);
}

bool Map2dPresenter::last_gpu_present_ok() const {
  return scenic_ ? scenic_->last_present_ok() : gpu_.last_present_ok();
}

bool Map2dPresenter::last_gpu_present_drew() const {
  return scenic_ ? scenic_->last_present_ok() : gpu_.last_present_drew();
}

void Map2dPresenter::note_surface_reset() {
  if (!scenic_) {
    gpu_.note_surface_reset();
  }
}

uint64_t Map2dPresenter::layout_build_count() const {
  return scenic_ ? 0 : cache_.layout_build_count();
}

bool Map2dPresenter::last_present_reused_layout() const {
  return scenic_ ? false : cache_.last_present_reused_layout();
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px,
                           bool fill_background) const {
  // Nested UpdateWindow→WM_PAINT can re-enter while scenic sync holds the
  // mutex; recursive_mutex covers that. Product (non-scenic) skips the lock
  // entirely so layout skew cannot treat an adjacent field as a mutex.
  if (prefer_map2d_scenic()) {
    std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_) {
      (void)fill_background;
      sync_scenic(static_cast<uint32_t>(width_px),
                  static_cast<uint32_t>(height_px));
      scenic_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                         static_cast<uint32_t>(height_px));
      return;
    }
  }
  hdc_.paint(hdc, width_px, height_px, fill_background);
}

void Map2dPresenter::paint_annotation_overlay(HDC hdc, int width_px,
                                              int height_px) const {
  // Product selection strokes are MapPass overlays. HDC annotation remains
  // for FORCE_GDI / ContentMapView harness faces only.
  if (!scenic_) {
    hdc_.paint_annotation_overlay(hdc, width_px, height_px);
  }
}

void Map2dPresenter::paint_flash_overlay(HDC hdc, int width_px,
                                         int height_px) const {
  if (!scenic_) {
    hdc_.paint_flash_overlay(hdc, width_px, height_px);
  }
}

bool Map2dPresenter::export_bmp(const std::string& path, int width_px,
                                int height_px) const {
  // Product carto / hillshade gates (map2d.china, browse soft score) use the
  // MapIR+GDI path. Scenic Map2dEngine fill is a matrix cell — flat green +
  // black strokes without hillshade/gold roads — and must not overwrite the
  // showcase BMP when FORCE_GDI_MAP_OVERLAY (browser_main showcase default).
  const bool force_gdi_carto = []() {
    const char* e = base::switch_cstr("force-gdi-map-overlay");
    return e && e[0] == '1' && e[1] == '\0';
  }();
  if (prefer_map2d_scenic() && !force_gdi_carto) {
    std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_) {
      sync_scenic(static_cast<uint32_t>(width_px),
                  static_cast<uint32_t>(height_px));
      return scenic_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                                 static_cast<uint32_t>(height_px));
    }
  }
  return hdc_.export_bmp(path, width_px, height_px);
}

size_t Map2dPresenter::basemap_tiles_drawn() const {
  return scenic_ ? 0 : hdc_.basemap_tiles_drawn();
}

}  // namespace content
