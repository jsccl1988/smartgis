// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Geometry walks, mesh/quad emit helpers, and line tessellation options.

#ifndef GIS_VISTA_DETAIL_LAYOUT_GEOM_MESH_H_
#define GIS_VISTA_DETAIL_LAYOUT_GEOM_MESH_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "vista/frame/frame.h"
#include "vista/world/terrain/mesh/tessellate.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {

// Tessellation grain: below this, stay serial (pool overhead).
inline constexpr size_t kParallelTessMinGeoms = 2;
// Prefer auto grain (span / (workers*4)); fixed grain=1 oversubscribed Debug.
inline constexpr size_t kParallelTessGrain = 0;

inline constexpr int kCircleSegments = 32;
inline constexpr float kPi = 3.14159265f;

const SymbolAsset* find_symbol(const LayoutInput& in, const std::string& id);
bool layer_uses_batch(const gis::style::StyleLayer& layer,
                      const LayerBatch& batch);

bool next_codepoint(const std::string& text, size_t* index, uint32_t* cp);
void push_quad(DrawItem* item, float x, float y, float w, float h, float u0,
               float v0, float u1, float v1);
DrawItem mesh_item(const vista::TessMesh& mesh, DrawKind kind, uint32_t rgba,
                   float opacity);

LineCap parse_cap(const std::string& cap);
LineJoin parse_join(const std::string& join);
LineTessOptions line_options(const gis::style::ResolvedPaint& paint,
                             double wupp);

template <class Fn>
void for_each_line(const OGRGeometry* geom, Fn&& fn) {
  if (!geom || geom->IsEmpty()) {
    return;
  }
  OGRGeometry* raw = const_cast<OGRGeometry*>(geom);
  const OGRwkbGeometryType type = wkbFlatten(raw->getGeometryType());
  if (type == wkbLineString) {
    if (auto* line = dynamic_cast<OGRLineString*>(raw)) {
      fn(line);
    }
    return;
  }
  if (type == wkbPolygon) {
    auto* poly = dynamic_cast<OGRPolygon*>(raw);
    if (!poly) {
      return;
    }
    if (OGRLinearRing* ring = poly->getExteriorRing()) {
      fn(ring);
    }
    for (int i = 0; i < poly->getNumInteriorRings(); ++i) {
      if (OGRLinearRing* ring = poly->getInteriorRing(i)) {
        fn(ring);
      }
    }
    return;
  }
  if (auto* coll = dynamic_cast<OGRGeometryCollection*>(raw)) {
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      for_each_line(coll->getGeometryRef(i), fn);
    }
  }
}

const OGRLineString* first_line(const OGRGeometry* geom);
bool anchor_xy(const OGRGeometry* geom, double* x, double* y);
void emit_circle(DrawItem* item, double cx, double cy, double radius);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_DETAIL_LAYOUT_GEOM_MESH_H_
