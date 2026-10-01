// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_COMPOSER_H_
#define LEGACY_RENDER_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_COMPOSER_H_

#include <cstdint>
#include <vector>

#include "gpu/compositor/frame/frame.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/common/surface/composer/submit.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/dib.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/owned.h"

namespace render {
namespace detail {

// Soft buf→buf compose via gpu::detail::blend_render_pass (src-over).
// Color-key TransparentBlt is mapped to alpha=0 on matching pixels.
// When src and dest sizes differ, nearest-neighbor scales into dest size
// before blending.
bool blit_surfaces(Rhi2dSurface& src, Rhi2dSurface& dst, int dest_x, int dest_y,
                   int dest_w, int dest_h, int src_x, int src_y, int src_w,
                   int src_h, bool color_key, COLORREF key);

// Copies |buf| pixels into an owned single-pass CompositorFrame (safe across
// async GPU-process submit).
bool make_compositor_frame(const Rhi2dSurface& buf,
                           gpu::detail::CompositorFrame* out);

// In-process software execute: blend root pass into |out_bgra| (BGRA8).
bool blend_frame_to_bgra(const gpu::detail::CompositorFrame& frame,
                         std::vector<uint8_t>* out_bgra);

// Builds an owned CompositorFrame and invokes registered sink(s).
// Returns false when no sink is set or fill/submit fails.
bool submit_surface(const Rhi2dSurface& buf);

// HWND present (GDI BitBlt / StretchBlt / TransparentBlt).
long present_to_hwnd(Rhi2dOwnedSurface& src, int dest_org_x, int dest_org_y,
                     int dest_w, int dest_h, int src_org_x, int src_org_y,
                     int op = SRCCOPY);
long present_to_hwnd(Rhi2dOwnedSurface& src, int dest_org_x, int dest_org_y,
                     int dest_w, int dest_h, int src_org_x, int src_org_y,
                     int src_w, int src_h, Rhi2dBlitMode mode,
                     int op = SRCCOPY,
                     // Key must NOT match clear ocean (170,211,223) or the HWND
                     // stays white when the whole buffer matches the key.
                     COLORREF clr = RGB(255, 255, 255));

// Buf→buf compose (soft color-key or GDI BitBlt/TransparentBlt fallback).
long blit_owned_to(Rhi2dOwnedSurface& src, Rhi2dOwnedSurface& target,
                   int dest_org_x, int dest_org_y, int dest_w, int dest_h,
                   int src_org_x, int src_org_y, int op = SRCCOPY);
long blit_owned_to(Rhi2dOwnedSurface& src, Rhi2dOwnedSurface& target,
                   int dest_org_x, int dest_org_y, int dest_w, int dest_h,
                   int src_org_x, int src_org_y, int src_w, int src_h,
                   Rhi2dBlitMode mode = Rhi2dBlitMode::kColorKey,
                   int op = SRCCOPY, COLORREF clr = RGB(255, 255, 255));

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_COMMON_SURFACE_COMPOSER_COMPOSER_H_
