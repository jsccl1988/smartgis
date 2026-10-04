// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_PROFILE_H_
#define GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_PROFILE_H_

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// CPU dispatch floor: match vista DEM hillshade (`kParallelShadeMinPixels`).
inline constexpr int kParallelGradientMinPixels = 4096;
inline constexpr int kParallelGradientMinRows = 8;
// GlobalNThreadPoolExecutor default size (see GlobalThreadPoolExecutor).
inline constexpr int kDemGradientPoolThreads = 16;

inline constexpr int kGradientDispatchAuto = 0;
inline constexpr int kGradientDispatchSerial = 1;
inline constexpr int kGradientDispatchParallel = 2;

// Last compute_dem_gradient timing snapshot (always recorded; dumped when
// SMT_ANALYSIS_PROFILE=1).
struct DemGradientProfile {
  int width = 0;
  int height = 0;
  int pixels = 0;
  int threads = 1;
  double dispatch_ms = 0.0;
  double compute_ms = 0.0;
  const char* backend = "serial";
};

GIS_EXPORT bool dem_gradient_profile_enabled();
GIS_EXPORT void set_dem_gradient_dispatch_override(int mode);
GIS_EXPORT int dem_gradient_dispatch_override();
GIS_EXPORT DemGradientProfile last_dem_gradient_profile();
GIS_EXPORT void record_dem_gradient_profile(const DemGradientProfile& snap);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_DEM_GRADIENT_PROFILE_H_
