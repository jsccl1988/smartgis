// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_SERVICE_AREA_H_
#define GIS_ANALYSIS_NETWORK_SERVICE_AREA_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/network/line_network.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct ServiceAreaResult {
  bool ok = false;
  // Interleaved x,y pairs per edge segment; edge breaks marked by NaN,NaN.
  std::vector<double> edge_xy;
  int edge_count = 0;
  std::string error;
};

GIS_EXPORT ServiceAreaResult run_service_area(const LineNetwork& net,
                                              int source,
                                              double cutoff);

// JSON: network, output, start_x, start_y, cutoff,
// optional weight_field / oneway_field / turns_path / barriers_path.
GIS_EXPORT bool run_service_area_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_SERVICE_AREA_H_
