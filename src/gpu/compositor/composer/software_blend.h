// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_COMPOSITOR_COMPOSER_SOFTWARE_BLEND_H_
#define GPU_COMPOSITOR_COMPOSER_SOFTWARE_BLEND_H_

#include "gpu/compositor/frame/frame.h"

#include <cstdint>
#include <vector>

namespace gpu {
namespace detail {

// Src-over the pass onto transparent black. Header-only deps: frame.h.
// Safe for in-process callers (e.g. legacy GDI buffer) without OutputSurface.
bool blend_render_pass(const RenderPass& pass, uint32_t width_px,
                       uint32_t height_px, std::vector<uint8_t>* dst);

}  // namespace detail
}  // namespace gpu

#endif  // GPU_COMPOSITOR_COMPOSER_SOFTWARE_BLEND_H_
