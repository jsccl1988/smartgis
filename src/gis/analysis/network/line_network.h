// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_LINE_NETWORK_H_
#define GIS_ANALYSIS_NETWORK_LINE_NETWORK_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Build options for an OGR line network (directed + optional turns/barriers).
struct NetworkBuildOptions {
  std::string weight_field;
  std::string capacity_field;
  std::string oneway_field;
  std::string turns_path;
  std::string barriers_path;
  double snap = 1e-6;
};

// Directed edge on a snapped node graph.
struct NetworkEdge {
  int from = -1;
  int to = -1;
  double cost = 0.0;
  double capacity = 1.0;
  int feature_id = -1;
};

// Forbidden move from→via→to (node ids).
struct TurnBan {
  int from = -1;
  int via = -1;
  int to = -1;
};

// Shared in-memory road / utility line graph for network solvers.
struct GIS_EXPORT LineNetwork {
  std::vector<std::pair<double, double>> nodes;
  std::vector<NetworkEdge> edges;
  // Outgoing edge indices per node.
  std::vector<std::vector<int>> out_edges;
  std::vector<TurnBan> turn_bans;
  std::vector<uint8_t> node_barrier;
  std::vector<uint8_t> edge_barrier;
  std::string error;

  bool ok() const { return error.empty() && !edges.empty(); }

  int nearest_node(double x, double y) const;
  bool is_node_blocked(int node) const;
  bool is_edge_blocked(int edge) const;
  bool is_turn_banned(int from, int via, int to) const;
};

// Load LineString/MultiLineString layers from `network_path` into a directed
// graph. Empty oneway_field → bidirectional (legacy cost_path behavior).
GIS_EXPORT LineNetwork build_line_network(std::string_view network_path,
                                          const NetworkBuildOptions& options);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_LINE_NETWORK_H_
