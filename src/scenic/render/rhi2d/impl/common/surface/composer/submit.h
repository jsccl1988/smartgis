// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_SUBMIT_H_
#define SCENIC_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_SUBMIT_H_

#include <cstdint>
#include <functional>

#include "gpu/compositor/frame/frame.h"
#include "scenic/scenic_impl_export.h"

namespace scenic {
namespace detail {

// Host (GPU process / map present) registers a submit sink.
// Final display compose must run in the GPU process; this only hands off IR.
using Rhi2dCompositorSubmitFn =
    std::function<bool(const gpu::detail::CompositorFrame& frame)>;

void set_compositor_submit(Rhi2dCompositorSubmitFn fn);
void clear_compositor_submit();
bool has_compositor_submit();

// Snapshot of registered sinks (C++ + C ABI). Caller holds no lock.
struct CompositorSubmitSinks {
  Rhi2dCompositorSubmitFn frame_fn;
  using BgraFn = bool (*)(const uint8_t* bgra, uint32_t width_px,
                          uint32_t height_px, uint32_t stride_bytes,
                          void* user);
  BgraFn bgra_fn = nullptr;
  void* bgra_user = nullptr;
};

CompositorSubmitSinks snapshot_compositor_submit();

}  // namespace detail
}  // namespace scenic

// C ABI for GetProcAddress from the GPU process (scenic_impl.dll).
extern "C" {

typedef bool (*Rhi2dBgraSubmitFn)(const uint8_t* bgra, uint32_t width_px,
                                   uint32_t height_px, uint32_t stride_bytes,
                                   void* user);

#if defined(SMT_GDI_COMPOSE_STATIC)
void Rhi2dSetBgraSubmit(Rhi2dBgraSubmitFn fn, void* user);
void Rhi2dClearBgraSubmit(void);
#else
LEGACY_RENDER_EXPORT void Rhi2dSetBgraSubmit(Rhi2dBgraSubmitFn fn,
                                               void* user);
LEGACY_RENDER_EXPORT void Rhi2dClearBgraSubmit(void);
#endif

}  // extern "C"

#endif  // SCENIC_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_SUBMIT_H_
