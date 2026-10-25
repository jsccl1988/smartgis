// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Static BuiltinOpEntry tables. run points at domain kernels; this TU only
// owns the read-only directory rows.

#include "gis/analysis/ops/runner/op_table.h"

#include "gis/analysis/geochem/op/idw_op.h"
#include "gis/analysis/geochem/op/stats_op.h"
#include "gis/analysis/geology/op/stratum_ops.h"
#include "gis/analysis/geometry/fit.h"
#include "gis/analysis/network/apsp.h"
#include "gis/analysis/network/cost_path.h"
#include "gis/analysis/network/max_flow.h"
#include "gis/analysis/network/service_area.h"
#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/dem/flood_fill.h"
#include "gis/analysis/raster/dem/storm_surge.h"
#include "gis/analysis/raster/dem/storm_surge_stats.h"
#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_smooth.h"

namespace gis {
namespace detail {
namespace {

constexpr BuiltinOpEntry kNetworkOps[] = {
    {"native.cost_path", "Least-cost / shortest path", &run_cost_path_op},
    {"native.service_area", "Network service area (cost cutoff)",
     &run_service_area_op},
    {"native.apsp", "Multi-source network cost matrix", &run_apsp_op},
    {"native.max_flow", "Directed network max flow", &run_max_flow_op},
};

constexpr BuiltinOpEntry kRasterDemOps[] = {
    {"native.flood_fill", "DEM inundation flood fill", &run_flood_fill_op},
    {"native.storm_surge", "Coastal storm-surge inundation", &run_storm_surge_op},
    {"native.storm_surge_stats",
     "Storm-surge inundation area / depth-class / overlap stats",
     &run_storm_surge_stats_op},
    {"native.dem_gradient", "DEM slope/aspect (Eigen Map)", &run_dem_gradient_op},
};

constexpr BuiltinOpEntry kRasterFilterOps[] = {
    {"native.raster_convolve", "Raster convolution (box3)", &run_raster_convolve_op},
    {"native.raster_smooth", "Raster Laplace smooth (Eigen SparseLU)",
     &run_raster_smooth_op},
};

constexpr BuiltinOpEntry kGeometryFitOps[] = {
    {"native.fit_line", "Least-squares 2D line fit (Eigen SVD)", &run_fit_line_op},
    {"native.fit_plane", "Least-squares 3D plane fit (Eigen SVD)", &run_fit_plane_op},
    {"native.affine_align", "2D affine alignment (Eigen least squares)",
     &run_affine_align_op},
    {"native.similarity_align", "2D similarity align (Umeyama)", &run_similarity_align_op},
    {"native.rigid_align", "2D rigid align (Umeyama, unit scale)", &run_rigid_align_op},
    {"native.pca_axes", "PCA principal axes of point set", &run_pca_axes_op},
    {"native.oriented_bbox", "Oriented bbox (PCA or min rotated rect)",
     &run_oriented_bbox_op},
};

// Geology ops live in namespace gis (not detail).
bool run_stratum_interpolate_bridge(std::string_view args_json) {
  return ::gis::run_stratum_interpolate_op(args_json);
}

bool run_stratum_prism_volume_bridge(std::string_view args_json) {
  return ::gis::run_stratum_prism_volume_op(args_json);
}

constexpr BuiltinOpEntry kGeologyOps[] = {
    {"native.stratum_interpolate", "Stratum surface TIN from borehole CSV",
     &run_stratum_interpolate_bridge},
    {"native.stratum_prism_volume", "Stratum prism volume (top/bottom)",
     &run_stratum_prism_volume_bridge},
};

constexpr BuiltinOpEntry kGeochemOps[] = {
    {"native.geochem_stats", "Geochem sample stats + histogram", &run_stats_op},
    {"native.geochem_idw", "Geochem IDW anomaly surface", &run_idw_op},
};

}  // namespace

std::span<const BuiltinOpEntry> network_op_table() {
  return kNetworkOps;
}

std::span<const BuiltinOpEntry> raster_dem_op_table() {
  return kRasterDemOps;
}

std::span<const BuiltinOpEntry> raster_filter_op_table() {
  return kRasterFilterOps;
}

std::span<const BuiltinOpEntry> geometry_fit_op_table() {
  return kGeometryFitOps;
}

std::span<const BuiltinOpEntry> geology_op_table() {
  return kGeologyOps;
}

std::span<const BuiltinOpEntry> geochem_op_table() {
  return kGeochemOps;
}

}  // namespace detail
}  // namespace gis
