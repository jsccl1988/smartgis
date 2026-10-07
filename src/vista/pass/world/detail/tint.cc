// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/detail/tint.h"

#include "vista/component/world/instance/paint.h"

namespace vista {
namespace detail {

void apply_paint_scalars(const gis::style::ResolvedPaint& paint, GpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  mesh->line_width = paint.line_width;
  mesh->circle_radius = paint.circle_radius;
  rgba_from_resolved_paint(paint, &mesh->solid_r, &mesh->solid_g, &mesh->solid_b,
                           &mesh->solid_a);
}

bool average_point_rgba(const Instance& inst, float* r, float* g, float* b) {
  if (inst.point_rgba.empty() || inst.point_rgba.size() < 4) {
    return false;
  }
  double sr = 0.0;
  double sg = 0.0;
  double sb = 0.0;
  const size_t pn = inst.point_rgba.size() / 4;
  for (size_t pi = 0; pi < pn; ++pi) {
    sr += inst.point_rgba[pi * 4];
    sg += inst.point_rgba[pi * 4 + 1];
    sb += inst.point_rgba[pi * 4 + 2];
  }
  if (r) {
    *r = static_cast<float>(sr / (pn * 255.0));
  }
  if (g) {
    *g = static_cast<float>(sg / (pn * 255.0));
  }
  if (b) {
    *b = static_cast<float>(sb / (pn * 255.0));
  }
  return true;
}

void apply_default_point_tint(GpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  mesh->solid_r = 220.f / 255.f;
  mesh->solid_g = 90.f / 255.f;
  mesh->solid_b = 40.f / 255.f;
  mesh->solid_a = 1.f;
}

void apply_untextured_terrain_tint(GpuMesh* mesh) {
  if (!mesh) {
    return;
  }
  mesh->solid_r = 0.22f;
  mesh->solid_g = 0.58f;
  mesh->solid_b = 0.20f;
  mesh->solid_a = 1.f;
}

}  // namespace detail
}  // namespace vista
