// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_PRISM_VOLUME_H_
#define GIS_ANALYSIS_GEOLOGY_PRISM_VOLUME_H_

#include <string>
#include <string_view>

#include "gis/analysis/geology/borehole.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Coarse prism volume between top and bottom stratum contacts.
struct PrismVolumeResult {
  bool ok = false;
  double volume = 0;  // map units^3
  std::string stratum_id;
  std::string error;
};

// Average paired thickness * convex-hull (or bbox) XY area. Coarse MVP.
GIS_EXPORT PrismVolumeResult prism_volume_between(
    const BoreholeSet& holes,
    std::string_view top_stratum_id,
    std::string_view bottom_stratum_id);

// JSON: input (CSV), top_stratum_id, bottom_stratum_id, output (JSON volume).
GIS_EXPORT bool run_stratum_prism_volume_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_PRISM_VOLUME_H_
