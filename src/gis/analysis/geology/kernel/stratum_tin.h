// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_KERNEL_STRATUM_TIN_H_
#define GIS_ANALYSIS_GEOLOGY_KERNEL_STRATUM_TIN_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/geology/model/borehole.h"
#include "gis/gis_export.h"

namespace gis {

// Interpolated stratum surface as a triangle mesh (map XYZ).
struct StratumTin {
  bool ok = false;
  std::string stratum_id;
  std::vector<double> xyz;        // packed x,y,z per vertex
  std::vector<uint32_t> indices;  // triangles (3 indices each)
  std::string error;
};

// Build a Delaunay surface TIN for one stratum_id from borehole contacts.
GIS_EXPORT StratumTin interpolate_stratum_tin(const BoreholeSet& holes,
                                              std::string_view stratum_id);

}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_KERNEL_STRATUM_TIN_H_
