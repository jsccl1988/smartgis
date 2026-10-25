// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/cost_path.h"

#include <string>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/ops/runner/ops_args.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {
namespace {

PathAlgorithm parse_algorithm(std::string_view name) {
  if (name == "bidirectional" || name == "bi" || name == "bidijkstra") {
    return PathAlgorithm::kBidirectional;
  }
  if (name == "astar" || name == "a_star" || name == "a*") {
    return PathAlgorithm::kAStar;
  }
  return PathAlgorithm::kDijkstra;
}

}  // namespace

PathResult run_cost_path_ex(std::string_view network_path,
                                double start_x,
                                double start_y,
                                double end_x,
                                double end_y,
                                const NetworkBuildOptions& options,
                                PathAlgorithm algorithm) {
  PathResult out;
  const LineNetwork net = build_line_network(network_path, options);
  if (!net.ok()) {
    out.error = net.error.empty() ? "build_failed" : net.error;
    return out;
  }
  PathQuery q;
  q.source = net.nearest_node(start_x, start_y);
  q.target = net.nearest_node(end_x, end_y);
  q.algorithm = algorithm;
  q.prefer_rightmost = (algorithm == PathAlgorithm::kDijkstra);
  q.hint_dx = end_x - start_x;
  q.hint_dy = end_y - start_y;
  return run_shortest_path(net, q);
}

PathResult run_cost_path(std::string_view network_path,
                             double start_x,
                             double start_y,
                             double end_x,
                             double end_y,
                             std::string_view weight_field) {
  NetworkBuildOptions opt;
  opt.weight_field = std::string(weight_field);
  return run_cost_path_ex(network_path, start_x, start_y, end_x, end_y, opt,
                          PathAlgorithm::kDijkstra);
}

bool write_path_geojson(std::string_view output_path, const PathResult& path) {
  if (!path.ok || path.xy.size() < 2 || output_path.empty()) {
    return false;
  }
  OGRLineString line;
  for (size_t i = 0; i + 1 < path.xy.size(); i += 2) {
    line.addPoint(path.xy[i], path.xy[i + 1]);
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  const std::string out(output_path);
  VSIUnlink(out.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(out.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("path", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  OGRFieldDefn cost("cost", OFTReal);
  layer->CreateField(&cost);
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
  if (!feat) {
    return false;
  }
  feat->SetGeometry(&line);
  feat->SetField("cost", path.total_cost);
  return layer->CreateFeature(feat.get()) == OGRERR_NONE;
}

bool run_cost_path_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!decode_op_args(args_json, &args)) {
    return false;
  }
  std::string network;
  std::string output;
  if (!op_arg_string(args, "network", &network) ||
      !op_arg_string(args, "output", &output)) {
    return false;
  }
  double start_x = 0;
  double start_y = 0;
  double end_x = 0;
  double end_y = 0;
  if (!op_arg_double(args, "start_x", &start_x) ||
      !op_arg_double(args, "start_y", &start_y) ||
      !op_arg_double(args, "end_x", &end_x) ||
      !op_arg_double(args, "end_y", &end_y)) {
    return false;
  }
  NetworkBuildOptions opt;
  op_arg_string(args, "weight_field", &opt.weight_field);
  op_arg_string(args, "oneway_field", &opt.oneway_field);
  op_arg_string(args, "turns_path", &opt.turns_path);
  op_arg_string(args, "barriers_path", &opt.barriers_path);
  op_arg_string(args, "capacity_field", &opt.capacity_field);
  std::string algorithm;
  op_arg_string(args, "algorithm", &algorithm);
  const CostPathResult path =
      run_cost_path_ex(network, start_x, start_y, end_x, end_y, opt,
                       parse_algorithm(algorithm));
  if (!path.ok) {
    return false;
  }
  return write_path_geojson(output, path);
}

}  // namespace detail
}  // namespace gis
