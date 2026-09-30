// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/compositor/composer/software_composer.h"

#include "gpu/compositor/composer/software_blend.h"

#include <vector>

namespace gpu {
namespace detail {

bool SoftwareComposer::draw_frame(OutputSurface* surface,
                                  const CompositorFrame& frame) {
  if (!surface || frame.width_px == 0 || frame.height_px == 0 ||
      frame.render_pass_list.empty() ||
      frame.render_pass_list.back().quad_list.empty()) {
    return false;
  }
  std::vector<uint8_t> pixels;
  if (!blend_render_pass(frame.render_pass_list.back(), frame.width_px,
                         frame.height_px, &pixels)) {
    return false;
  }
  return surface->upload_bgra(pixels.data(), frame.width_px * 4u);
}

}  // namespace detail
}  // namespace gpu
