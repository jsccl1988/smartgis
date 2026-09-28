// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_OPS_OPS_RUNNER_H_
#define GIS_ANALYSIS_OPS_OPS_RUNNER_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

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

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_OPS_RUNNER_H_
