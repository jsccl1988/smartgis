// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_RASTER_SYNTHETIC_H_
#define VISTA_TERRAIN_DEM_RASTER_SYNTHETIC_H_

namespace vista {
namespace detail {

// Deprecated stand-in height field used by DemRaster::fill_synthetic_china.
float synthetic_meters(double lon, double lat);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_RASTER_SYNTHETIC_H_
