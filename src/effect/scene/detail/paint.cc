// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/scene/detail/paint.h"

namespace effect {
namespace scene {
namespace detail {

gis::LineCap line_cap_from_paint(const std::string& cap) {
  if (cap == "round") {
    return gis::LineCap::kRound;
  }
  if (cap == "square") {
    return gis::LineCap::kSquare;
  }
  return gis::LineCap::kButt;
}

gis::LineJoin line_join_from_paint(const std::string& join) {
  if (join == "round") {
    return gis::LineJoin::kRound;
  }
  if (join == "bevel") {
    return gis::LineJoin::kBevel;
  }
  return gis::LineJoin::kMiter;
}

// MapLibre line-* (pixels) → LineTessOptions. Dash lengths are converted to
// world units with the same world_units_per_pixel as stroke width.
gis::LineTessOptions line_options_from_paint(
    const gis::style::ResolvedPaint& paint, double world_units_per_pixel) {
  gis::LineTessOptions options;
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

void apply_paint_scalars(const gis::style::ResolvedPaint& paint,
                         GpuScene::GpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  mesh->line_width = paint.line_width;
  mesh->circle_radius = paint.circle_radius;
  rgba_from_resolved_paint(paint, &mesh->solid_r, &mesh->solid_g,
                           &mesh->solid_b, &mesh->solid_a);
}

}  // namespace detail

void rgba_from_resolved_paint(const gis::style::ResolvedPaint& paint, float* r,
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

}  // namespace scene
}  // namespace effect
