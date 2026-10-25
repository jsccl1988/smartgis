// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_APSP_H_
#define GIS_ANALYSIS_NETWORK_APSP_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gis/analysis/network/line_network.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct ApspPair {
  int from = -1;
  int to = -1;
  double cost = 0.0;
  bool reachable = false;
};

struct ApspResult {
  bool ok = false;
  std::vector<ApspPair> pairs;
  std::string error;
};

// Multi-source Dijkstra among `sources` (APSP subset, not full Floyd).
GIS_EXPORT ApspResult run_apsp(const LineNetwork& net,
                               const std::vector<int>& sources);

GIS_EXPORT bool run_apsp_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_APSP_H_
