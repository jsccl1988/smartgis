// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_TRANSFORM_H_
#define GIS_GEO_OPS_TRANSFORM_H_

#include <cstdint>

#include "gis/gis_export.h"

class OGRGeometry;

namespace geo {

// In-place EPSG→EPSG vertex transform via CoordinateTransform.
// Returns false when CRS is invalid or a vertex fails.
GIS_EXPORT bool transform_inplace(OGRGeometry* geom, int32_t src_epsg, int32_t dst_epsg);

}  // namespace geo

#endif  // GIS_GEO_OPS_TRANSFORM_H_
