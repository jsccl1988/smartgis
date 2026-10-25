// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_OP_STATS_OP_H_
#define GIS_ANALYSIS_GEOCHEM_OP_STATS_OP_H_

#include <string_view>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// JSON: input (CSV), element, optional bins, k_sigma, correlate, output (JSON).
GIS_EXPORT bool run_stats_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_OP_STATS_OP_H_
