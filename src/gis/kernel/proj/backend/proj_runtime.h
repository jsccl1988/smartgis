// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_PROJ_PROJ_RUNTIME_H_
#define ALGORITHM_PROJ_PROJ_RUNTIME_H_

#include <proj.h>

#include <string>

// Process-wide PROJ 9 context, search paths, and CRS-to-CRS pipelines.
// PJ* is never stored in sdb/crs; only this adapter owns PROJ objects.
namespace proj {
namespace detail {

// IUGG 1975 / Krasovsky a,b used by the old plugin Gauss/Lambert UI.
inline constexpr double k_iugg1975_a = 6378140.0;
inline constexpr double k_iugg1975_b = 6356755.2882;
inline constexpr double k_krasovsky_a = 6378245.0;
inline constexpr double k_krasovsky_b = 6356863.0187730473;

// 6-degree Gauss-Kruger central meridian (degrees) from a geographic
// longitude. Zone n covers [(n-1)*6, n*6); lon_0 = n*6 - 3.
double gauss_kruger_central_meridian(double lon_deg);

// PROJ strings for the plugin zone / standard-parallel fields. Not a
// class hierarchy; callers pass the result to load_projection_string.
std::string make_longlat_crs(double a, double b);
std::string make_tmerc_crs(double a, double b, double lon_0);
std::string make_lcc_crs(double a,
                         double b,
                         double lat_1,
                         double lat_2,
                         double lat_0,
                         double lon_0);

PJ_CONTEXT* context();

void set_search_path(const char* proj_lib, const char* rel_to_path);

// Create a CRS from EPSG, WKT, or a PROJ string. Caller does not own the
// result when using cached_crs; use create_crs when the caller will destroy.
PJ* create_crs(const char* definition);

// proj_create_crs_to_crs + proj_normalize_for_visualization. Cached by
// definition strings. Do not proj_destroy the returned object.
PJ* cached_pipeline(const char* source_crs, const char* target_crs);

// Same as cached_pipeline but keyed by existing CRS objects (Projection).
PJ* cached_pipeline_from_pj(PJ* source_crs, PJ* target_crs);

void forget_pj(PJ* crs);

int trans_xy(PJ* pipeline, PJ_DIRECTION direction, double* x, double* y);

bool is_geographic_crs(const PJ* crs);

}  // namespace detail
}  // namespace proj

#endif  // ALGORITHM_PROJ_PROJ_RUNTIME_H_
