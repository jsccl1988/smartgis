// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Read-only builtin op tables. Each domain owns a static span; the runner
// only chains them. run == nullptr means the GeoJSON file geometry path.

#ifndef GIS_ANALYSIS_OPS_RUNNER_OP_TABLE_H_
#define GIS_ANALYSIS_OPS_RUNNER_OP_TABLE_H_

#include <span>
#include <string_view>

namespace gis {
namespace detail {

struct BuiltinOpEntry {
  const char* id;
  const char* title;
  bool (*run)(std::string_view args_json);
};

std::span<const BuiltinOpEntry> geometry_file_op_table();
std::span<const BuiltinOpEntry> network_op_table();
std::span<const BuiltinOpEntry> raster_dem_op_table();
std::span<const BuiltinOpEntry> raster_filter_op_table();
std::span<const BuiltinOpEntry> geometry_fit_op_table();
std::span<const BuiltinOpEntry> geology_op_table();
std::span<const BuiltinOpEntry> geochem_op_table();
std::span<const BuiltinOpEntry> geometry_layer_op_table();

const BuiltinOpEntry* find_builtin_op(std::string_view id);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_RUNNER_OP_TABLE_H_
