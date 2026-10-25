// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_SHORTEST_PATH_H_
#define GIS_ANALYSIS_NETWORK_SHORTEST_PATH_H_

#include <string>
#include <vector>

#include "gis/analysis/network/line_network.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

enum class PathAlgorithm {
  kDijkstra = 0,
  kBidirectional = 1,
  kAStar = 2,
};

// Least-cost path result (map-coordinate polyline).
struct PathResult {
  bool ok = false;
  double total_cost = 0.0;
  std::vector<double> xy;
  std::string error;
};

struct PathQuery {
  int source = -1;
  int target = -1;
  // Optional right-hand tie-break (China RHT) using travel direction hint.
  bool prefer_rightmost = false;
  double hint_dx = 0.0;
  double hint_dy = 0.0;
  PathAlgorithm algorithm = PathAlgorithm::kDijkstra;
};

GIS_EXPORT PathResult run_shortest_path(const LineNetwork& net,
                                        const PathQuery& query);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_SHORTEST_PATH_H_
