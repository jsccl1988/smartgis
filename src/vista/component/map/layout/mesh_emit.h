// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU mesh helpers: quads, tess meshes, circle fans, line options.
// No OGR walks and no RHI.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_MESH_EMIT_H_
#define VISTA_COMPONENT_MAP_LAYOUT_MESH_EMIT_H_

#include <string>

#include "gis/style/style_types.h"
#include "vista/component/map/draw.h"
#include "vista/component/map/layout.h"
#include "vista/mesh/detail/mesh_types.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

inline constexpr int kCircleSegments = 32;
// kPi lives in mesh_types.h (double) — shared with line_tess / fill_tess.

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

#endif  // VISTA_COMPONENT_MAP_LAYOUT_MESH_EMIT_H_
