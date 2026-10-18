// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Imports and exports WKT text as OGR geometries. Callers own the geometry
// through OgrGeom. This unit does not run spatial algorithms.

#ifndef GIS_ANALYSIS_OPS_WKT_WKT_CODEC_H_
#define GIS_ANALYSIS_OPS_WKT_WKT_CODEC_H_

#include <memory>
#include <string>
#include <string_view>

#include "gis/analysis/ops/ir/ogr_geom.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

GIS_EXPORT OgrGeom geometry_from_wkt(std::string_view text, std::string* error);

GIS_EXPORT std::string geometry_to_wkt(const OGRGeometry* geom);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_WKT_WKT_CODEC_H_
