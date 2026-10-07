// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_GRAPH_SHELL_OVERLAY_EFFECT_H_
#define RENDER_GRAPH_SHELL_OVERLAY_EFFECT_H_

#include <cstdint>

#include "render/graph/frame_graph.h"
#include "render/render_export.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"

namespace render {
namespace graph {

// Fullscreen src-over of DrawRequest.shell-equivalent BGRA into the map
// swapchain (kOverlay). Generation skip reuses the uploaded texture so
// unchanged HUD does not re-upload. Empty / size mismatch is a no-op success.
// DrawHost shell_generation and abandon_gpu timing stay caller-owned.
class RENDER_EXPORT ShellOverlayEffect final : public Effect {
 public:
  ShellOverlayEffect();
  ~ShellOverlayEffect() override;

  ShellOverlayEffect(const ShellOverlayEffect&) = delete;
  ShellOverlayEffect& operator=(const ShellOverlayEffect&) = delete;

  // Bind pixels for the next record(). Pointers must outlive present().
  void bind(const ui::gfx::ShellRaster& shell, uint64_t shell_generation);
  void clear();

  // Drop GPU handles without destroy_* (Device may already be gone, or the
  // caller is on the display thread before a full rebuild). Safe when the UI
  // thread only marks the presenter stale.
  void abandon_gpu();

  bool has_shell() const { return shell_.bgra != nullptr; }

  EffectSlot slot() const override;
  bool record(const RecordContext& ctx) override;

 private:
  void release_device_resources();
  bool ensure_pipeline(rhi::Device* device);
  bool ensure_texture(rhi::Device* device, uint32_t width_px,
                      uint32_t height_px);

  ui::gfx::ShellRaster shell_{};
  uint64_t shell_generation_ = 0;

  rhi::Device* device_ = nullptr;
  rhi::Pipeline* pipeline_ = nullptr;
  rhi::Texture* texture_ = nullptr;
  rhi::Buffer* vb_ = nullptr;
  rhi::Buffer* ib_ = nullptr;
  uint64_t uploaded_generation_ = 0;
  uint32_t uploaded_w_ = 0;
  uint32_t uploaded_h_ = 0;
};

}  // namespace graph
}  // namespace render

#endif  // RENDER_GRAPH_SHELL_OVERLAY_EFFECT_H_
