// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_MAX_FLOW_H_
#define GIS_ANALYSIS_NETWORK_MAX_FLOW_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/network/line_network.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

struct MaxFlowResult {
  bool ok = false;
  double flow = 0.0;
  // Edge indices that carry positive flow.
  std::vector<int> used_edges;
  std::vector<double> edge_flow;
  std::string error;
};

GIS_EXPORT MaxFlowResult run_max_flow(const LineNetwork& net, int source,
                                      int sink);

GIS_EXPORT bool run_max_flow_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_MAX_FLOW_H_
