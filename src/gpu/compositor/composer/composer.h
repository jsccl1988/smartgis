// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_COMPOSER_COMPOSER_H_
#define GPU_COMPOSITOR_COMPOSER_COMPOSER_H_

#include "gpu/compositor/frame/frame.h"
#include "gpu/device/adapter_id.h"
#include "gpu/display/output_surface.h"

#include <memory>

namespace gpu {
namespace detail {

enum class ComposeBackend {
  kSoftware = 0,
  kRhi = 1,
};

// Executes one CompositorFrame onto OutputSurface on a single adapter.
// Final compose runs only inside the GPU process — never in shell.
class FrameComposer {
 public:
  virtual ~FrameComposer() = default;
  virtual ComposeBackend backend() const = 0;
  virtual AdapterId adapter() const = 0;
  virtual bool draw_frame(OutputSurface* surface,
                          const CompositorFrame& frame) = 0;
};

// Default is kRhi when GPU_COMPOSE is unset or unknown; only explicit
// "software" selects kSoftware.
ComposeBackend select_compose_backend();
void set_compose_backend(ComposeBackend backend);
void clear_compose_backend_override();

// Never returns null. kRhi may fall back to software until RHI init succeeds
// for |adapter| (sticky per-adapter via GpuDeviceHub).
std::unique_ptr<FrameComposer> make_frame_composer(ComposeBackend backend,
                                                   AdapterId adapter);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_COMPOSER_COMPOSER_H_
