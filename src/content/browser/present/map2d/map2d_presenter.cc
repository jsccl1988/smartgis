// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/host/gdi/gdi_primitives.h"
#include "content/browser/present/host/scenic/scenic_scene_bind.h"
#include "content/browser/present/map2d/frame/export/map2d_mapir_export.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

namespace content {
namespace {

void fill_ocean(HDC hdc, int width_px, int height_px) {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  RECT rc{0, 0, width_px, height_px};
  detail::gdi_fill_rect(hdc, rc, detail::rgba_to_colorref(0xFFAAD3DFu));
}

}  // namespace

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
}

void Map2dPresenter::invalidate_frame_cache() {
  // Invariant: visibility / style / feature edits change ContentFingerprint and
  // must still drop the published MapIR here. Extent / pan / fly must not —
  // prepare_for_present already keys camera separately (InteractiveReuse).
  if (!cache_.content_differs_from_cache()) {
    return;
  }
  gpu_.invalidate_frame_cache();
}

void Map2dPresenter::force_invalidate_frame_cache() {
  gpu_.invalidate_frame_cache();
}

bool Map2dPresenter::warm_frame_cache(uint32_t width_px, uint32_t height_px) {
  if (width_px == 0 || height_px == 0) {
    return false;
  }
  vista::MapIR unused;
  Map2dFrameCache::CameraKey cam{};
  return cache_.ensure_full(width_px, height_px, &unused, &cam);
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
      const bool ok = scenic_->present();
      if (ok) {
        ++scenic_present_count_;
      }
      return ok;
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
  return scenic_ ? scenic_present_count_ : cache_.layout_build_count();
}

bool Map2dPresenter::last_present_reused_layout() const {
  // Always reflect Map2dFrameCache (gpu / FlyCube path). scenic_ is lazily
  // null when MAP2D_ENGINE!=scenic but the old scenic_ ? false guard made
  // layout-cache unit tests read false after StaticReuse prepares.
  return cache_.last_present_reused_layout();
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px,
                           bool fill_background) const {
  if (prefer_map2d_scenic()) {
    std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_) {
      (void)fill_background;
      sync_scenic(static_cast<uint32_t>(width_px),
                  static_cast<uint32_t>(height_px));
      if (scenic_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                             static_cast<uint32_t>(height_px))) {
        ++scenic_present_count_;
      }
      return;
    }
  }
  // Harness BitBlt / motion_gate: FORCE_GDI_MAP_OVERLAY paints MapIR→HDC.
  // HWND paint must not call ensure_full (sync rebuild) — it deadlocks the
  // UI thread against still export / Display. Dual-speed prepare + snap only;
  // still.cc re-frames the embed after 1280 export so pixels match.
  if (base::switch_is_one("force-gdi-map-overlay") && hdc && width_px > 0 &&
      height_px > 0) {
    Map2dFrameCache& cache = const_cast<Map2dFrameCache&>(cache_);
    const uint32_t w = static_cast<uint32_t>(width_px);
    const uint32_t h = static_cast<uint32_t>(height_px);
    Map2dFrameCache::PresentAction action =
        Map2dFrameCache::PresentAction::kStaticReuse;
    (void)cache.prepare_for_present(w, h, &action);
    Map2dFrameCache::PresentSnapshot snap;
    if (cache.snap(&snap) && snap.frame) {
      const Map2dFrameCache::CameraKey live = cache.live_camera_key(w, h);
      if (detail::paint_mapir_hdc(
              hdc, *snap.frame, live,
              [&cache](uint32_t key, std::vector<uint8_t>* rgba, int* rw,
                       int* rh) {
                return cache.load_raster(key, rgba, rw, rh);
              })) {
        return;
      }
    }
  }
  // Product DXGI MapPass is the interactive SoT; HWND fill is ocean while
  // waiting for present when GDI overlay is off.
  if (fill_background) {
    fill_ocean(hdc, width_px, height_px);
  }
}

void Map2dPresenter::paint_annotation_overlay(HDC hdc, int width_px,
                                              int height_px) const {
  (void)hdc;
  (void)width_px;
  (void)height_px;
}

void Map2dPresenter::paint_flash_overlay(HDC hdc, int width_px,
                                         int height_px) const {
  (void)hdc;
  (void)width_px;
  (void)height_px;
}

bool Map2dPresenter::export_bmp_via_scenic(const std::string& path, int width_px,
                                           int height_px) const {
  std::unique_ptr<scenic::Engine> eng(scenic::create_map2d_engine());
  if (!eng || path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  scenic::SessionDesc desc;
  desc.width_px = static_cast<uint32_t>(width_px);
  desc.height_px = static_cast<uint32_t>(height_px);
  eng->initialize(desc);
  const double scale = frame_ ? frame_->scale() : 1.0;
  std::vector<scenic::Vertex2> xy;
  std::vector<scenic::DrawItem> items;
  detail::fill_scenic_draw_items(scene_, scale, &xy, &items);
  eng->bind_view(detail::scenic_view_from_frame(frame_));
  eng->bind_draw_items(items.data(), static_cast<uint32_t>(items.size()));
  return eng->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                         static_cast<uint32_t>(height_px));
}

bool Map2dPresenter::export_bmp_via_mapir(const std::string& path, int width_px,
                                          int height_px) const {
  if (path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  // export_bmp is const for callers; layout bake mutates the shared cache.
  Map2dFrameCache& cache = const_cast<Map2dFrameCache&>(cache_);
  vista::MapIR frame_snap;
  Map2dFrameCache::CameraKey cam{};
  if (!cache.ensure_full(static_cast<uint32_t>(width_px),
                         static_cast<uint32_t>(height_px), &frame_snap,
                         &cam)) {
    return false;
  }
  return detail::export_mapir_bmp(
      frame_snap, cam,
      [&cache](uint32_t key, std::vector<uint8_t>* rgba, int* w, int* h) {
        return cache.load_raster(key, rgba, w, h);
      },
      path);
}

bool Map2dPresenter::export_bmp(const std::string& path, int width_px,
                                int height_px) const {
  if (prefer_map2d_scenic()) {
    std::lock_guard<std::recursive_mutex> lock(scenic_mu_);
    ensure_scenic();
    if (scenic_) {
      sync_scenic(static_cast<uint32_t>(width_px),
                  static_cast<uint32_t>(height_px));
      return scenic_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                                 static_cast<uint32_t>(height_px));
    }
  }
  // Product / FlyCube SoT: MapIR with hillshade (not scenic flat fill).
  return export_bmp_via_mapir(path, width_px, height_px);
}

size_t Map2dPresenter::basemap_tiles_drawn() const {
  return 0;
}

}  // namespace content
