// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// ResolvedPaint 鈫?RGBA / LineTessOptions helpers for GpuScene upload.

#ifndef EFFECT_SCENE_DETAIL_PAINT_H_
#define EFFECT_SCENE_DETAIL_PAINT_H_

#include <cstdint>
#include <string>

#include "gis/carto/style/style_types.h"
#include "vista/world/terrain/mesh/tessellate.h"
#include "vista/scene/scene.h"

namespace vista {
namespace detail {

void argb_to_rgba(uint32_t argb, float opacity, float* r, float* g, float* b,
                  float* a);

void apply_paint_scalars(const gis::style::ResolvedPaint& paint,
                         GpuScene::GpuMesh* mesh);

vista::LineCap line_cap_from_paint(const std::string& cap);
vista::LineJoin line_join_from_paint(const std::string& join);

vista::LineTessOptions line_options_from_paint(
    const gis::style::ResolvedPaint& paint, double world_units_per_pixel);

}  // namespace detail
}  // namespace vista

#endif  // EFFECT_SCENE_DETAIL_PAINT_H_
