// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/ops_runner.h"

#include <vector>

namespace gis {
namespace detail {
namespace {

const std::vector<BuiltinOpDesc> kCatalog = {
    {"native.buffer", "Buffer"},
    {"native.clip", "Clip"},
    {"native.centroid", "Centroid"},
    {"native.envelope", "Envelope"},
    {"native.intersection", "Intersection"},
    {"native.union", "Union"},
    {"native.difference", "Difference"},
    {"native.symmetric_difference", "Symmetric difference"},
    {"native.simplify", "Simplify"},
    {"native.convex_hull", "Convex hull"},
    {"native.boundary", "Boundary"},
    {"native.cost_path", "Least-cost / shortest path"},
    {"native.flood_fill", "DEM inundation flood fill"},
    {"native.storm_surge", "Coastal storm-surge inundation"},
    {"native.storm_surge_stats",
     "Storm-surge inundation area / depth-class / overlap stats"},
    {"native.fit_line", "Least-squares 2D line fit (Eigen SVD)"},
    {"native.fit_plane", "Least-squares 3D plane fit (Eigen SVD)"},
    {"native.affine_align", "2D affine alignment (Eigen least squares)"},
    {"native.dem_gradient", "DEM slope/aspect (Eigen Map)"},
    {"native.raster_convolve", "Raster convolution (box3)"},
    {"native.raster_smooth", "Raster Laplace smooth (Eigen SparseLU)"},
    {"native.stratum_interpolate", "Stratum surface TIN from borehole CSV"},
    {"native.stratum_prism_volume", "Stratum prism volume (top/bottom)"},
    {"native.geochem_stats", "Geochem sample stats + histogram"},
    {"native.geochem_idw", "Geochem IDW anomaly surface"},
};

}  // namespace

const std::vector<BuiltinOpDesc>& builtin_op_catalog() {
  return kCatalog;
}

}  // namespace detail
}  // namespace gis
