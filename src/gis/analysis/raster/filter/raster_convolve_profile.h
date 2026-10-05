// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_PROFILE_H_
#define GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_PROFILE_H_

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// CPU dispatch floor: match vista DEM hillshade (tiny 5x5 tests stay serial).
inline constexpr int kParallelConvolveMinPixels = 4096;
inline constexpr int kParallelConvolveMinRows = 8;

inline constexpr int kConvolveDispatchAuto = 0;
inline constexpr int kConvolveDispatchSerial = 1;
inline constexpr int kConvolveDispatchParallel = 2;

// Last convolve_raster timing snapshot (always recorded; logged when
// ANALYSIS_PROFILE=1).
struct RasterConvolveProfile {
  int kernel_w = 0;
  int kernel_h = 0;
  int kernel_size = 0;
  int pixels = 0;
  double dispatch_ms = 0.0;
  double compute_ms = 0.0;
  const char* backend = "serial";
};

GIS_EXPORT bool analysis_profile_enabled();
GIS_EXPORT void set_convolve_dispatch_override(int mode);
GIS_EXPORT int convolve_dispatch_override();
GIS_EXPORT RasterConvolveProfile last_convolve_profile();

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_FILTER_RASTER_CONVOLVE_PROFILE_H_
