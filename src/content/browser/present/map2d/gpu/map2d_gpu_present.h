// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_GPU_PRESENT_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_GPU_PRESENT_H_

#include <cstdint>
#include <memory>

#include "content/browser/present/host/shell_overlay_effect.h"
#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "content/content_export.h"
#include "ui/gfx/raster/shell_raster.h"

namespace vista {
class FramePass;
}

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace content {

class MapScene;
class ViewFrame;

// Lightweight present-path counters for FPS self-evolve (reset per bench).
struct Map2dGpuPresentProfile {
  uint64_t skip = 0;
  uint64_t full = 0;
  uint64_t action_rebuild = 0;
  uint64_t action_interactive = 0;
  uint64_t action_settle = 0;
  uint64_t action_static = 0;
};

CONTENT_EXPORT Map2dGpuPresentProfile map2d_gpu_present_profile();
CONTENT_EXPORT void reset_map2d_gpu_present_profile();

// GPU present path for 2D maps: Pass lifetime + shell overlay.
// MapFrame dual-speed cache lives on Map2dFrameCache (shared with GDI).
class Map2dGpuPresent {
 public:
  Map2dGpuPresent();
  ~Map2dGpuPresent();

  Map2dGpuPresent(const Map2dGpuPresent&) = delete;
  Map2dGpuPresent& operator=(const Map2dGpuPresent&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame,
            Map2dFrameCache* cache);

  // Drop Pass / overlay GPU state and invalidate shared frame cache.
  void invalidate_frame_cache();

  // |shell| / |shell_generation| mirror gpu::DrawRequest.shell (src-over HUD).
  bool present(render::rhi::Device* device, uint32_t width_px,
               uint32_t height_px, const ui::gfx::ShellRaster* shell = nullptr,
               uint64_t shell_generation = 0);

  bool last_present_ok() const { return last_present_ok_; }
  // True when the last present() call recorded Pass / graph::present (not a
  // StaticReuse swapchain keep). show_shell must wait on a drew frame so a
  // skip after clear cannot satisfy the first-map pump.
  bool last_present_drew() const { return last_present_drew_; }
  // After DXGI Resize/initialize the swapchain is clear — clear the skip latch
  // so the next present re-submits Pass (StaticReuse must not keep hollow).
  void note_surface_reset() {
    last_present_ok_ = false;
    last_present_drew_ = false;
    last_shell_generation_ = 0;
    last_had_shell_ = false;
  }
  uint64_t layout_build_count() const;
  bool last_present_reused_layout() const;

 private:
  bool present_frame(render::rhi::Device* device,
                     const Map2dFrameCache::CameraKey& cam, bool record_all,
                     const ui::gfx::ShellRaster* shell,
                     uint64_t shell_generation);

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  Map2dFrameCache* cache_ = nullptr;
  std::unique_ptr<vista::FramePass> map2d_pass_;
  detail::ShellOverlayEffect shell_overlay_;
  bool last_present_ok_ = false;
  bool last_present_drew_ = false;
  // Last shell generation successfully presented (StaticReuse skip).
  uint64_t last_shell_generation_ = 0;
  bool last_had_shell_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_GPU_MAP2D_GPU_PRESENT_H_
