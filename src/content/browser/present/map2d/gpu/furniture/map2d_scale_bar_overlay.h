// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_

#include <cstdint>

#include "vista/component/map/draw.h"

namespace content {
namespace detail {

// Appends a bottom-left scale bar (pixel-space strokes + text) for the live
// camera. Drawn every present so the bar stays visible during pan/zoom.
void append_map2d_scale_bar(uint32_t width_px, uint32_t height_px, double min_x,
                            double min_y, double max_x, double max_y,
                            vista::MapIR* ir);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_
