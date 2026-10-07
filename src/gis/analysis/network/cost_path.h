// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_NETWORK_COST_PATH_H_
#define GIS_ANALYSIS_NETWORK_COST_PATH_H_

#include <string>
#include <string_view>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Result of a least-cost path on an OGR line network.
struct CostPathResult {
  bool ok = false;
  double total_cost = 0.0;
  // Interleaved x,y for the reconstructed path (map coords).
  std::vector<double> xy;
  std::string error;
};

// Build an undirected graph from OGR LineString/MultiLineString features,
// run Dijkstra from the nearest node to (start_x,start_y) toward (end_x,end_y).
// Among equal / near-equal cost paths, prefer the rightmost route
// (China drive-on-right). weight_field empty → Euclidean segment length.
GIS_EXPORT CostPathResult run_cost_path(std::string_view network_path,
                                        double start_x,
                                        double start_y,
                                        double end_x,
                                        double end_y,
                                        std::string_view weight_field);

// Write path as GeoJSON LineString. Returns false on I/O failure.
GIS_EXPORT bool write_path_geojson(std::string_view output_path,
                                   const CostPathResult& path);

// JSON args: network, output, start_x, start_y, end_x, end_y,
// optional weight_field. Writes GeoJSON path to output.
GIS_EXPORT bool run_cost_path_op(std::string_view args_json);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_NETWORK_COST_PATH_H_
