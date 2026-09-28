// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_SOFTWARE_RENDERER_H_
#define GPU_COMPOSITOR_SOFTWARE_RENDERER_H_

#include "gpu/compositor/compositor_frame.h"
#include "gpu/compositor/frame_composer.h"
#include "gpu/device/adapter_id.h"
#include "gpu/display/output_surface.h"

#include <cstdint>
#include <vector>

namespace gpu {
namespace detail {

// CPU FrameComposer for one adapter. Blends the root pass, then presents once.
// Runs only inside the GPU process (per-device software fallback).
class SoftwareComposer : public FrameComposer {
 public:
  explicit SoftwareComposer(AdapterId adapter = kAdapterPrimary)
      : adapter_(adapter) {}

  ComposeBackend backend() const override { return ComposeBackend::kSoftware; }
  AdapterId adapter() const override { return adapter_; }

  // Blends root-pass quads into one BGRA buffer and calls upload_bgra once.
  // A null surface, a zero size, or an empty root pass returns false.
  bool draw_frame(OutputSurface* surface,
                  const CompositorFrame& frame) override;

 private:
  AdapterId adapter_ = kAdapterPrimary;
};

// Legacy name kept for call sites that only need blend helpers.
using SoftwareRenderer = SoftwareComposer;

// Src-over the root pass onto transparent black. Used by tile fallback before
// the display present, and by draw_frame.
bool blend_render_pass(const RenderPass& pass, uint32_t width_px,
                       uint32_t height_px, std::vector<uint8_t>* dst);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_SOFTWARE_RENDERER_H_
