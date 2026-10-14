// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_HUD_HUD_PASS_H_
#define VISTA_PASS_WORLD_HUD_HUD_PASS_H_

#include <cstdint>
#include <vector>

#include "render/rhi/rhi.h"
#include "vista/pass/world/hud/hud_ir.h"
#include "vista/vista_export.h"

namespace vista {

// Device-thread HUD overlay: bind HudFrame → raster BGRA → upload → src-over.
// Product Scene3d path uses this instead of HDC soft paint / FORCE_GDI.
class VISTA_EXPORT HudPass {
 public:
  HudPass() = default;
  ~HudPass();

  HudPass(const HudPass&) = delete;
  HudPass& operator=(const HudPass&) = delete;

  // Take ownership of the next frame IR. Empty frame clears the overlay.
  void bind(HudFrame frame);
  void clear();

  bool has_frame() const { return !frame_.empty(); }
  uint64_t generation() const { return frame_.generation; }

  void abandon_gpu();
  void release(render::rhi::Device* device);

  // Record into an open CommandList (caller owns render pass lifecycle when
  // using record_draws). Prefer HudEffect via graph::present.
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width_px, uint32_t height_px);

 private:
  bool ensure_pipeline(render::rhi::Device* device);
  bool ensure_texture(render::rhi::Device* device, uint32_t width_px,
                      uint32_t height_px);
  void release_device_resources();

  HudFrame frame_;
  std::vector<uint8_t> bgra_;
  uint64_t raster_generation_ = 0;

  render::rhi::Device* device_ = nullptr;
  render::rhi::Pipeline* pipeline_ = nullptr;
  render::rhi::Texture* texture_ = nullptr;
  render::rhi::Buffer* vb_ = nullptr;
  render::rhi::Buffer* ib_ = nullptr;
  uint64_t uploaded_generation_ = 0;
  uint32_t uploaded_w_ = 0;
  uint32_t uploaded_h_ = 0;
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_HUD_HUD_PASS_H_
