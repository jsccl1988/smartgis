// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_GDI_COMPOSE_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_GDI_COMPOSE_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "gpu/compositor/frame/frame.h"
#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi2d/impl/gdi/surface/surface_pool.h"

namespace render {
namespace detail {

// Phase 1: buf→buf compose via gpu::detail::blend_render_pass (src-over).
// Color-key TransparentBlt is mapped to alpha=0 on matching pixels.
// When src and dest sizes differ, nearest-neighbor scales into dest size
// before blending (DrawQuad has no stretch).
bool compose_buf_to_buf(GdiSurface& src, GdiSurface& dst, int dest_x,
                        int dest_y, int dest_w, int dest_h, int src_x,
                        int src_y, int src_w, int src_h, bool /*stretch*/,
                        bool color_key, COLORREF key);

// Snapshot |buf| as a single-pass CompositorFrame with external BGRA pointers
// (valid only while |buf| bits stay alive).
bool fill_compositor_frame(const GdiSurface& buf,
                           gpu::detail::CompositorFrame* out);

// Same as fill_compositor_frame but copies pixels into pass.image_data so the
// frame is safe across async GPU-process submit.
bool fill_compositor_frame_owned(const GdiSurface& buf,
                                 gpu::detail::CompositorFrame* out);

// Phase 2: host (GPU process / map present) registers a submit sink.
// Final display compose must run in the GPU process; this only hands off IR.
using GdiCompositorSubmitFn =
    std::function<bool(const gpu::detail::CompositorFrame& frame)>;

void set_gdi_compositor_submit(GdiCompositorSubmitFn fn);
void clear_gdi_compositor_submit();
bool has_gdi_compositor_submit();

// Builds an owned CompositorFrame and invokes the registered sink(s).
// Returns false when no sink is set or fill/submit fails.
bool submit_compositor_frame(const GdiSurface& buf);

// In-process software execute: blend root pass into |out_bgra| (BGRA8).
bool compose_frame_to_bgra(const gpu::detail::CompositorFrame& frame,
                           std::vector<uint8_t>* out_bgra);

}  // namespace detail
}  // namespace render

// C ABI for GetProcAddress from the GPU process (legacy_render.dll).
extern "C" {

typedef bool (*SmtGdiBgraSubmitFn)(const uint8_t* bgra, uint32_t width_px,
                                   uint32_t height_px, uint32_t stride_bytes,
                                   void* user);

#if defined(SMT_GDI_COMPOSE_STATIC)
void SmtGdiSetBgraSubmit(SmtGdiBgraSubmitFn fn, void* user);
void SmtGdiClearBgraSubmit(void);
#else
LEGACY_RENDER_EXPORT void SmtGdiSetBgraSubmit(SmtGdiBgraSubmitFn fn,
                                              void* user);
LEGACY_RENDER_EXPORT void SmtGdiClearBgraSubmit(void);
#endif

}  // extern "C"

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_BUFFER_GDI_COMPOSE_H_
