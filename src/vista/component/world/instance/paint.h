// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// ResolvedPaint ->RGBA / LineTessOptions helpers for WorldPass upload.

#ifndef VISTA_COMPONENT_WORLD_INSTANCE_PAINT_H_
#define VISTA_COMPONENT_WORLD_INSTANCE_PAINT_H_

#include <cstdint>
#include <string>

#include "gis/style/style_types.h"
#include "vista/mesh/tessellate.h"
#include "vista/vista_export.h"

namespace vista {

// Maps ResolvedPaint layer type to RGBA. Multiplies the matching opacity into
// alpha. kBackground / kFill / unknown ->fill_*; kLine ->line_*; kCircle ->
// circle_*; kSymbol without bytes still returns fill_* as a fallback tint.
VISTA_EXPORT void rgba_from_resolved_paint(
    const gis::style::ResolvedPaint& paint, float* r, float* g, float* b,
    float* a);

namespace detail {

void argb_to_rgba(uint32_t argb, float opacity, float* r, float* g, float* b,
                  float* a);

vista::LineCap line_cap_from_paint(const std::string& cap);
vista::LineJoin line_join_from_paint(const std::string& join);

vista::LineTessOptions line_options_from_paint(
    const gis::style::ResolvedPaint& paint, double world_units_per_pixel);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_INSTANCE_PAINT_H_
