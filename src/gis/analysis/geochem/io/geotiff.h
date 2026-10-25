// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_IO_GEOTIFF_H_
#define GIS_ANALYSIS_GEOCHEM_IO_GEOTIFF_H_

#include <string_view>

#include "gis/analysis/geochem/idw/idw.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Write Float32 GeoTIFF of IDW values; optional Byte anomaly mask path.
GIS_EXPORT bool write_idw_geotiff(std::string_view output_path,
                                  const GeochemIdwResult& result,
                                  std::string_view mask_path);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_IO_GEOTIFF_H_
