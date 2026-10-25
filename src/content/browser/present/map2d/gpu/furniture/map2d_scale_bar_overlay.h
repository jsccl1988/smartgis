// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_

#include <cstdint>

#include "vista/component/map/draw.h"

namespace content {
namespace detail {

// Appends screen-fixed furniture for the live camera: bottom-left scale bar
// and top-center north arrow. Items are marked screen_fixed so MapPass
// re-places them on pan (geo-label DrawCache reuse must not move the HUD).
void append_map2d_scale_bar(uint32_t width_px, uint32_t height_px, double min_x,
                            double min_y, double max_x, double max_y,
                            vista::MapIR* ir);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_GPU_FURNITURE_MAP2D_SCALE_BAR_OVERLAY_H_
