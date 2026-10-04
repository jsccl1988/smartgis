// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// ResolvedPaint → RGBA / LineTessOptions helpers for WorldPass upload.

#ifndef VISTA_WORLD_PAINT_H_
#define VISTA_WORLD_PAINT_H_

#include <cstdint>
#include <string>

#include "gis/style/style_types.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

void argb_to_rgba(uint32_t argb, float opacity, float* r, float* g, float* b,
                  float* a);

vista::LineCap line_cap_from_paint(const std::string& cap);
vista::LineJoin line_join_from_paint(const std::string& join);

vista::LineTessOptions line_options_from_paint(
    const gis::style::ResolvedPaint& paint, double world_units_per_pixel);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_WORLD_PAINT_H_
