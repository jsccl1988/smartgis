// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOLOGY_OP_STRATUM_OPS_H_
#define GIS_ANALYSIS_GEOLOGY_OP_STRATUM_OPS_H_

#include <string_view>

#include "gis/gis_export.h"

namespace gis {

// JSON: input (CSV), stratum_id, output (JSON mesh). Returns false on failure.
GIS_EXPORT bool run_stratum_interpolate_op(std::string_view args_json);

// JSON: input (CSV), top_stratum_id, bottom_stratum_id, output (JSON volume).
GIS_EXPORT bool run_stratum_prism_volume_op(std::string_view args_json);

}  // namespace gis

#endif  // GIS_ANALYSIS_GEOLOGY_OP_STRATUM_OPS_H_
