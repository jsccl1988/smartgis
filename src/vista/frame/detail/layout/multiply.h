// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Luma multiply baked into a straight RGBA8 coverage. No RHI header.

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_MULTIPLY_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_MULTIPLY_H_

#include <cstdint>
#include <span>

#include "vista/vista_export.h"

namespace vista {

// Hillshade coverage into straight RGBA8 (tightly packed, length % 4 == 0).
// luma = 0.299R+0.587G+0.114B; m = (1-opacity)+opacity*luma;
// A < 160 -> 0, else 255. RGB channels scaled by m.
VISTA_EXPORT void apply_multiply_coverage(std::span<std::uint8_t> rgba,
                                        float opacity);

}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_MULTIPLY_H_
