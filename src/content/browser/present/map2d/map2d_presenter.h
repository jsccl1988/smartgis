// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/map2d/frame/cache/map2d_frame_cache.h"
#include "content/browser/present/map2d/gpu/present/map2d_gpu_present.h"
#include "content/content_export.h"
#include "scenic/engine.h"
#include "ui/gfx/raster/shell_raster.h"

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace content {

class GisScene;
class ViewFrame;

CONTENT_EXPORT bool prefer_map2d_scenic();

// Map2d facade: FlyCube MapPass GPU present (+ optional scenic matrix cell).
// FORCE_GDI_MAP_OVERLAY enables MapIR→HDC for harness BitBlt / motion_gate.
class Map2dPresenter {
 public:
  // Same heap-owning factory as Scene3dPresenter::create — do not embed
  // Map2dPresenter by value in BrowserSession (frame cache / scenic mutex
  // sizeof drift smashed the CRT heap).
  struct Deleter {
    void operator()(Map2dPresenter* p) const;
  };
  using Ptr = std::unique_ptr<Map2dPresenter, Deleter>;
  static Ptr create();

  Map2dPresenter();
  ~Map2dPresenter();

  Map2dPresenter(const Map2dPresenter&) = delete;
  Map2dPresenter& operator=(const Map2dPresenter&) = delete;

  void bind(const GisScene* scene, const ViewFrame* frame);

  Map2dFrameCache& frame_cache() { return cache_; }
  const Map2dFrameCache& frame_cache() const { return cache_; }
  Map2dGpuPresent& gpu() { return gpu_; }
  const Map2dGpuPresent& gpu() const { return gpu_; }

  // Forwarded to Map2dGpuPresent (selection flash pulse on MapPass).
  void set_flash_pulse(bool pulse) { gpu_.set_flash_pulse(pulse); }

  bool hosts_scenic_present() const;

  // Drops published MapIR / GPU latches only when scene/layer fingerprint
  // changed. Camera-only callers are no-ops (layout_builds_delta).
  void invalidate_frame_cache();
  // Unconditional MapIR / Pass drop (DefaultChina post-show, surface settle).
  // Fingerprint-stable invalidate_frame_cache is a no-op and cannot clear a
  // wrong first layout latched before final client fit.
  void force_invalidate_frame_cache();
  // Sync MapIR bake for |width_px|×|height_px| (harness still→stress handoff).
  // Call only when HWND paint is paused — not from WM_PAINT.
  bool warm_frame_cache(uint32_t width_px, uint32_t height_px);

  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);
  bool last_gpu_present_ok() const;
  bool last_gpu_present_drew() const;
  void note_surface_reset();
  // After an owned RHI device that fed present_gpu is shut down.
  void note_device_lost();
  uint64_t layout_build_count() const;
  bool last_present_reused_layout() const;

  // HWND path: scenic paint_hdc when MAP2D_ENGINE=scenic; MapIR→HDC when
  // FORCE_GDI_MAP_OVERLAY; otherwise ocean fill (product SoT is DXGI MapPass).
  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;
  // No-ops: selection/flash are MapPass MapIR overlays.
  void paint_annotation_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_flash_overlay(HDC hdc, int width_px, int height_px) const;

  // Scenic when MAP2D_ENGINE=scenic; else MapIR→BMP (align carto gold).
  bool export_bmp(const std::string& path, int width_px, int height_px) const;
  size_t basemap_tiles_drawn() const;

 private:
  void ensure_scenic() const;
  void sync_scenic(uint32_t width_px, uint32_t height_px) const;
  bool export_bmp_via_scenic(const std::string& path, int width_px,
                             int height_px) const;
  bool export_bmp_via_mapir(const std::string& path, int width_px,
                            int height_px) const;

  // gpu_ before cache_: Map2dFrameCache is large (MapIR / Arena). A sizeof
  // drift that wrote past cache_ used to smash map2d_pass_ (float 1.0f /
  // 0x3f800000 → AV in MapPass::abandon on BindPresenters).
  Map2dGpuPresent gpu_;
  Map2dFrameCache cache_;

  const GisScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  // Nested WM_PAINT under show/UpdateWindow re-enters paint/sync when
  // MAP2D_ENGINE=scenic. Product paint skips this lock. Same sizeof as
  // std::mutex on MSVC x64 (80) so member offsets stay layout-compatible.
  mutable std::recursive_mutex scenic_mu_;
  mutable std::unique_ptr<scenic::Engine> scenic_;
  // Scenic has no MapIR layout cache; count successful paint/present so
  // WaitFirstMapPresent / session probes see progress.
  mutable uint64_t scenic_present_count_ = 0;
  mutable std::vector<scenic::Vertex2> scenic_xy_;
  mutable std::vector<scenic::DrawItem> scenic_items_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
