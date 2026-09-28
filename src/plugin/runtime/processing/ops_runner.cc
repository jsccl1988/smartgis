// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/processing/ops_runner.h"

#include "gis/analysis/ops/ops_runner.h"

namespace plugin {

const std::vector<BuiltinOpDesc>& builtin_op_catalog() {
  static const std::vector<BuiltinOpDesc> kCatalog = [] {
    std::vector<BuiltinOpDesc> out;
    for (const auto& e : gis::detail::builtin_op_catalog()) {
      out.push_back({e.id, e.title});
    }
    return out;
  }();
  return kCatalog;
}

bool run_builtin_op(std::string_view processing_id,
                    std::string_view args_json) {
  return gis::detail::run_builtin_op(processing_id, args_json);
}

}  // namespace plugin
