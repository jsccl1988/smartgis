// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_COST_PATH_H_
#define GIS_ANALYSIS_NETWORK_COST_PATH_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/analysis/network/line_network.h"
#include "gis/analysis/network/shortest_path.h"
#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Alias kept for traffic plugin / gate ABI.
using CostPathResult = PathResult;

// Build an undirected (default) or directed graph from OGR lines and run a
// shortest / least-cost path. Among equal / near-equal cost paths, prefer the
// rightmost route (China drive-on-right) when using Dijkstra.
GIS_EXPORT PathResult run_cost_path(std::string_view network_path,
                                    double start_x,
                                    double start_y,
                                    double end_x,
                                    double end_y,
                                    std::string_view weight_field);

GIS_EXPORT PathResult run_cost_path_ex(
    std::string_view network_path,
    double start_x,
    double start_y,
    double end_x,
    double end_y,
    const NetworkBuildOptions& options,
    PathAlgorithm algorithm);

// Write path as GeoJSON LineString. Returns false on I/O failure.
GIS_EXPORT bool write_path_geojson(std::string_view output_path,
                                   const PathResult& path);

// JSON args: network, output, start_x, start_y, end_x, end_y,
// optional weight_field / oneway_field / turns_path / barriers_path /
// algorithm (dijkstra|bidirectional|astar).
GIS_EXPORT bool run_cost_path_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_COST_PATH_H_
