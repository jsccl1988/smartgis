// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_OPS_OPS_RUNNER_H_
#define GIS_ANALYSIS_OPS_OPS_RUNNER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

class OGRGeometry;

namespace gis {
namespace detail {

// Catalog entry for a native GeoJSON file → file operator.
struct BuiltinOpDesc {
  const char* id;
  const char* title;
};

GIS_EXPORT const std::vector<BuiltinOpDesc>& builtin_op_catalog();

// Run one operator synchronously (GeoJSON file → GeoJSON file).
// args_json keys: input, output, distance, clip.
GIS_EXPORT bool run_builtin_op(std::string_view processing_id,
                               std::string_view args_json);

// In-memory geometry kernel shared with the file operator.
// id is a catalog id: native.buffer, native.clip, native.intersection,
// native.union, native.difference, native.symmetric_difference,
// native.centroid, native.envelope, native.simplify, native.convex_hull,
// native.boundary. clip is required for binary set ops. distance is the
// buffer width or simplify tolerance. Caller owns the result.
GIS_EXPORT std::unique_ptr<OGRGeometry> apply_op(std::string_view id,
                                                  OGRGeometry* input,
                                                  OGRGeometry* clip,
                                                  double distance);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_OPS_RUNNER_H_
