// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROCESSING_OPS_RUNNER_H_
#define PLUGIN_PROCESSING_OPS_RUNNER_H_

#include <string>
#include <string_view>
#include <vector>

namespace plugin {

// Catalog entry (public product face). Implementation lives in
// gis/analysis/ops; this header only forwards.
struct BuiltinOpDesc {
  const char* id;
  const char* title;
};

const std::vector<BuiltinOpDesc>& builtin_op_catalog();

// Run one operator synchronously (GeoJSON file → GeoJSON file).
// args_json keys: input, output, distance, clip.
// Thin forward to gis::detail::run_builtin_op.
bool run_builtin_op(std::string_view processing_id, std::string_view args_json);

}  // namespace plugin

#endif  // PLUGIN_PROCESSING_OPS_RUNNER_H_
