// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/op_table.h"

#include "gis/analysis/layer/layer_ops.h"

namespace gis {
namespace detail {
namespace {

constexpr BuiltinOpEntry kGeometryLayerOps[] = {
    {"native.dissolve", "Dissolve by attribute key", &run_dissolve_op},
    {"native.clip_layer", "Clip layer by polygon layer", &run_clip_layer_op},
    {"native.spatial_join", "Spatial join (predicate + field copy)",
     &run_spatial_join_op},
    {"native.erase", "Erase overlay (GEOS difference)", &run_erase_op},
    {"native.identity", "Identity overlay (intersect + remainder)",
     &run_identity_op},
    {"native.split_line", "Split lines at nearby points", &run_split_line_op},
    {"native.cut_polygon", "Cut polygons by lines (node + polygonize)",
     &run_cut_polygon_op},
};

}  // namespace

std::span<const BuiltinOpEntry> geometry_layer_op_table() {
  return kGeometryLayerOps;
}

}  // namespace detail
}  // namespace gis
