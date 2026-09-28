// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_
#define APP_VIEWS_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_

#include <cstdint>

#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"

namespace app {
namespace detail {

// Fullscreen src-over of DrawRequest.shell-equivalent BGRA into the map
// swapchain (kOverlay). Generation skip reuses the uploaded texture so
// unchanged HUD does not re-upload. Empty / size mismatch is a no-op success.
class ShellOverlayEffect final : public render::graph::Effect {
 public:
  ShellOverlayEffect();
  ~ShellOverlayEffect() override;

  ShellOverlayEffect(const ShellOverlayEffect&) = delete;
  ShellOverlayEffect& operator=(const ShellOverlayEffect&) = delete;

  // Bind pixels for the next record(). Pointers must outlive present().
  void bind(const ui::gfx::ShellRaster& shell, uint64_t shell_generation);
  void clear();

  bool has_shell() const { return shell_.bgra != nullptr; }

  render::graph::EffectSlot slot() const override;
  bool record(const render::graph::RecordContext& ctx) override;

 private:
  void release_device_resources();
  bool ensure_pipeline(render::rhi::Device* device);
  bool ensure_texture(render::rhi::Device* device, uint32_t width_px,
                      uint32_t height_px);

  ui::gfx::ShellRaster shell_{};
  uint64_t shell_generation_ = 0;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Texture* texture_ = nullptr;
  render::rhi::Buffer* vb_ = nullptr;
  render::rhi::Buffer* ib_ = nullptr;
  uint64_t uploaded_generation_ = 0;
  uint32_t uploaded_w_ = 0;
  uint32_t uploaded_h_ = 0;
};

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_PRESENT_HOST_SHELL_OVERLAY_EFFECT_H_
