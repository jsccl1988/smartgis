// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_OP_IDW_OP_H_
#define GIS_ANALYSIS_GEOCHEM_OP_IDW_OP_H_

#include <string_view>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// JSON: input, element, output; optional cells, power, threshold, k_sigma,
// mask_output.
GIS_EXPORT bool run_idw_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_OP_IDW_OP_H_
