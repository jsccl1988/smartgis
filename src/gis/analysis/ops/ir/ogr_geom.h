// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Owns an OGR geometry created by OGRGeometryFactory or GEOS import.

#ifndef GIS_ANALYSIS_OPS_OGR_GEOM_H_
#define GIS_ANALYSIS_OPS_OGR_GEOM_H_

#include <memory>

#include "ogr_geometry.h"

namespace gis {
namespace detail {

struct OgrGeomDel {
  void operator()(OGRGeometry* geom) const {
    if (geom) {
      OGRGeometryFactory::destroyGeometry(geom);
    }
  }
};

using OgrGeom = std::unique_ptr<OGRGeometry, OgrGeomDel>;

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_OGR_GEOM_H_
