// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// OGR geometry walks for layout emit. No mesh building, no RHI.

#ifndef VISTA_MAP_LAYOUT_GEOM_WALK_H_
#define VISTA_MAP_LAYOUT_GEOM_WALK_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "gis/style/style_types.h"
#include "ogrsf_frmts.h"
#include "vista/map/batch.h"

class OGRGeometry;
class OGRLineString;

namespace vista {
namespace detail {

bool layer_uses_batch(const gis::style::StyleLayer& layer,
                      const LayerBatch& batch);

bool next_codepoint(const std::string& text, size_t* index, uint32_t* cp);

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

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MAP_LAYOUT_GEOM_WALK_H_
