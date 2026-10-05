// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/paint.h"

#include "vista/vista_export.h"

namespace vista {
namespace detail {

vista::LineCap line_cap_from_paint(const std::string& cap) {
  if (cap == "round") {
    return vista::LineCap::kRound;
  }
  if (cap == "square") {
    return vista::LineCap::kSquare;
  }
  return vista::LineCap::kButt;
}

vista::LineJoin line_join_from_paint(const std::string& join) {
  if (join == "round") {
    return vista::LineJoin::kRound;
  }
  if (join == "bevel") {
    return vista::LineJoin::kBevel;
  }
  return vista::LineJoin::kMiter;
}

// MapLibre line-* (pixels) → LineTessOptions. Dash lengths are converted to
// world units with the same world_units_per_pixel as stroke width.
vista::LineTessOptions line_options_from_paint(
    const gis::style::ResolvedPaint& paint, double world_units_per_pixel) {
  vista::LineTessOptions options;
  options.pixel_width = paint.line_width;
  options.world_units_per_pixel = world_units_per_pixel;
  options.cap = line_cap_from_paint(paint.line_cap);
  options.join = line_join_from_paint(paint.line_join);
  options.dasharray.clear();
  options.dasharray.reserve(paint.line_dasharray.size());
  for (float dash_px : paint.line_dasharray) {
    options.dasharray.push_back(static_cast<double>(dash_px) *
                                world_units_per_pixel);
  }
  return options;
}

void argb_to_rgba(uint32_t argb, float opacity, float* r, float* g, float* b,
                  float* a) {
  const float inv = 1.f / 255.f;
  if (r) {
    *r = static_cast<float>((argb >> 16) & 0xff) * inv;
  }
  if (g) {
    *g = static_cast<float>((argb >> 8) & 0xff) * inv;
  }
  if (b) {
    *b = static_cast<float>(argb & 0xff) * inv;
  }
  if (a) {
    const float base_a = static_cast<float>((argb >> 24) & 0xff) * inv;
    float op = opacity;
    if (op < 0.f) {
      op = 0.f;
    } else if (op > 1.f) {
      op = 1.f;
    }
    *a = base_a * op;
  }
}

}  // namespace detail

VISTA_EXPORT void rgba_from_resolved_paint(const gis::style::ResolvedPaint& paint, float* r,
                              float* g, float* b, float* a) {
  using gis::style::LayerType;
  uint32_t argb = paint.fill_color;
  float opacity = paint.fill_opacity;
  switch (paint.type) {
    case LayerType::kLine:
      argb = paint.line_color;
      opacity = paint.line_opacity;
      break;
    case LayerType::kCircle:
      argb = paint.circle_color;
      opacity = paint.circle_opacity;
      break;
    case LayerType::kBackground:
    case LayerType::kFill:
    case LayerType::kSymbol:
    case LayerType::kRaster:
    case LayerType::kUnknown:
    default:
      break;
  }
  detail::argb_to_rgba(argb, opacity, r, g, b, a);
}

}  // namespace vista
