// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/ops_runner.h"

#include "gis/analysis/ops/runner/op_table.h"

#include <vector>

namespace gis {
namespace detail {
namespace {

using TableFn = std::span<const BuiltinOpEntry> (*)();

constexpr TableFn kTables[] = {
    &geometry_file_op_table,
    &geometry_layer_op_table,
    &network_op_table,
    &raster_dem_op_table,
    &raster_filter_op_table,
    &geometry_fit_op_table,
    &geology_op_table,
    &geochem_op_table,
};

}  // namespace

const BuiltinOpEntry* find_builtin_op(std::string_view id) {
  for (TableFn table_fn : kTables) {
    for (const BuiltinOpEntry& entry : table_fn()) {
      if (id == entry.id) {
        return &entry;
      }
    }
  }
  return nullptr;
}

const std::vector<BuiltinOpDesc>& builtin_op_catalog() {
  static const std::vector<BuiltinOpDesc> kCatalog = [] {
    std::vector<BuiltinOpDesc> out;
    for (TableFn table_fn : kTables) {
      for (const BuiltinOpEntry& entry : table_fn()) {
        out.push_back({entry.id, entry.title});
      }
    }
    return out;
  }();
  return kCatalog;
}

}  // namespace detail
}  // namespace gis
