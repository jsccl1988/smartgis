// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GeoJSON file bytes to and from OGR geometries. No spatial algorithms.

#ifndef GIS_ANALYSIS_OPS_GEOJSON_GEOJSON_CODEC_H_
#define GIS_ANALYSIS_OPS_GEOJSON_GEOJSON_CODEC_H_

#include <memory>
#include <string>

class OGRGeometry;

namespace gis {
namespace detail {

std::unique_ptr<OGRGeometry> geometry_from_geojson(const std::string& path);

bool geometry_to_geojson(const std::string& path, OGRGeometry* geom);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_GEOJSON_GEOJSON_CODEC_H_
