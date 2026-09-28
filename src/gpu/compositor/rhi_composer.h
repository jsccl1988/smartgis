// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_RHI_COMPOSER_H_
#define GPU_COMPOSITOR_RHI_COMPOSER_H_

#include "gpu/compositor/frame_composer.h"
#include "gpu/device/adapter_id.h"

#include <cstdint>

namespace render {
namespace rhi {
class Buffer;
class Device;
class Pipeline;
class Texture;
}  // namespace rhi
}  // namespace render

namespace gpu {
namespace detail {

// Opt-in RHI FrameComposer for one AdapterId.
// Records GPU compose of kSolid / kBgra / replaces when a Device is live.
// Texture cache lives on GpuDeviceHub (per adapter). Present prefers shared
// NT import / compose-into-shared; else blend_render_pass + upload_bgra.
class RhiComposer : public FrameComposer {
 public:
  explicit RhiComposer(AdapterId adapter);
  ~RhiComposer() override;

  ComposeBackend backend() const override { return ComposeBackend::kRhi; }
  AdapterId adapter() const override { return adapter_; }

  bool draw_frame(OutputSurface* surface,
                  const CompositorFrame& frame) override;

 private:
  render::rhi::Texture* texture_for_quad(render::rhi::Device* device,
                                         const DrawQuad& quad);
  bool record_gpu_compose(render::rhi::Device* device,
                          const CompositorFrame& frame);
  bool present_to_surface(render::rhi::Device* device, OutputSurface* surface,
                          const uint8_t* bgra, uint32_t width_px,
                          uint32_t height_px);

  AdapterId adapter_ = kAdapterPrimary;
  uint32_t cache_generation_ = 0;
  render::rhi::Buffer* quad_ib_ = nullptr;
  render::rhi::Pipeline* solid_ = nullptr;
  render::rhi::Pipeline* textured_ = nullptr;
  render::rhi::Device* cached_device_ = nullptr;
};

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_RHI_COMPOSER_H_
