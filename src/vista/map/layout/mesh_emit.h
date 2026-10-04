// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU mesh helpers: quads, tess meshes, circle fans, line options.
// No OGR walks and no RHI.

#ifndef VISTA_MAP_LAYOUT_MESH_EMIT_H_
#define VISTA_MAP_LAYOUT_MESH_EMIT_H_

#include <string>

#include "gis/style/style_types.h"
#include "vista/map/draw.h"
#include "vista/map/layout.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

inline constexpr int kCircleSegments = 32;
inline constexpr float kPi = 3.14159265f;

const SymbolAsset* find_symbol(const LayoutInput& in, const std::string& id);

void push_quad(DrawItem* item, float x, float y, float w, float h, float u0,
               float v0, float u1, float v1);
DrawItem mesh_item(const vista::TessMesh& mesh, DrawKind kind, uint32_t rgba,
                   float opacity);

LineCap parse_cap(const std::string& cap);
LineJoin parse_join(const std::string& join);
LineTessOptions line_options(const gis::style::ResolvedPaint& paint,
                             double wupp);

void emit_circle(DrawItem* item, double cx, double cy, double radius);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MAP_LAYOUT_MESH_EMIT_H_
