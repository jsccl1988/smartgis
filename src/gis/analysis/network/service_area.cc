// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/service_area.h"

#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/ops/runner/ops_args.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {
namespace {

struct QueueItem {
  double cost = 0.0;
  int node = -1;
  int prev = -1;
  bool operator>(const QueueItem& o) const {
    if (cost != o.cost) {
      return cost > o.cost;
    }
    return node > o.node;
  }
};

bool write_edges_geojson(std::string_view output_path,
                         const ServiceAreaResult& area) {
  if (!area.ok || output_path.empty()) {
    return false;
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
  OGRLayer* layer =
      ds->CreateLayer("service_area", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  OGRLineString line;
  for (size_t i = 0; i + 1 < area.edge_xy.size(); i += 2) {
    const double x = area.edge_xy[i];
    const double y = area.edge_xy[i + 1];
    if (std::isnan(x) && std::isnan(y)) {
      if (line.getNumPoints() >= 2) {
        OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
        if (!feat) {
          return false;
        }
        feat->SetGeometry(&line);
        if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
          return false;
        }
      }
      line.empty();
      continue;
    }
    line.addPoint(x, y);
  }
  if (line.getNumPoints() >= 2) {
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    if (!feat) {
      return false;
    }
    feat->SetGeometry(&line);
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

}  // namespace

ServiceAreaResult run_service_area(const LineNetwork& net, int source,
                                   double cutoff) {
  ServiceAreaResult out;
  if (source < 0 || static_cast<size_t>(source) >= net.nodes.size()) {
    out.error = "no_nodes";
    return out;
  }
  if (!(cutoff > 0.0)) {
    out.error = "bad_cutoff";
    return out;
  }
  if (net.is_node_blocked(source)) {
    out.error = "blocked";
    return out;
  }

  const size_t n = net.nodes.size();
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  std::vector<uint8_t> edge_used(net.edges.size(), 0);
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq;
  dist[static_cast<size_t>(source)] = 0.0;
  pq.push({0.0, source, -1});

  while (!pq.empty()) {
    const QueueItem top = pq.top();
    pq.pop();
    if (top.cost > dist[static_cast<size_t>(top.node)] + 1e-15) {
      continue;
    }
    if (top.cost > cutoff) {
      continue;
    }
    if (static_cast<size_t>(top.node) >= net.out_edges.size()) {
      continue;
    }
    for (int ei : net.out_edges[static_cast<size_t>(top.node)]) {
      if (net.is_edge_blocked(ei)) {
        continue;
      }
      const NetworkEdge& e = net.edges[static_cast<size_t>(ei)];
      if (net.is_node_blocked(e.to)) {
        continue;
      }
      if (top.prev >= 0 && net.is_turn_banned(top.prev, top.node, e.to)) {
        continue;
      }
      const double nd = top.cost + e.cost;
      if (nd <= cutoff + 1e-15) {
        edge_used[static_cast<size_t>(ei)] = 1;
      }
      if (nd < dist[static_cast<size_t>(e.to)]) {
        dist[static_cast<size_t>(e.to)] = nd;
        if (nd <= cutoff) {
          pq.push({nd, e.to, top.node});
        }
      }
    }
  }

  for (size_t ei = 0; ei < edge_used.size(); ++ei) {
    if (!edge_used[ei]) {
      continue;
    }
    const NetworkEdge& e = net.edges[ei];
    const auto& a = net.nodes[static_cast<size_t>(e.from)];
    const auto& b = net.nodes[static_cast<size_t>(e.to)];
    if (!out.edge_xy.empty()) {
      out.edge_xy.push_back(std::numeric_limits<double>::quiet_NaN());
      out.edge_xy.push_back(std::numeric_limits<double>::quiet_NaN());
    }
    out.edge_xy.push_back(a.first);
    out.edge_xy.push_back(a.second);
    out.edge_xy.push_back(b.first);
    out.edge_xy.push_back(b.second);
    ++out.edge_count;
  }
  if (out.edge_count == 0) {
    out.error = "empty_area";
    return out;
  }
  out.ok = true;
  return out;
}

bool run_service_area_op(std::string_view args_json) {
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
  double cutoff = 0;
  if (!op_arg_double(args, "start_x", &start_x) ||
      !op_arg_double(args, "start_y", &start_y) ||
      !op_arg_double(args, "cutoff", &cutoff)) {
    return false;
  }
  NetworkBuildOptions opt;
  op_arg_string(args, "weight_field", &opt.weight_field);
  op_arg_string(args, "oneway_field", &opt.oneway_field);
  op_arg_string(args, "turns_path", &opt.turns_path);
  op_arg_string(args, "barriers_path", &opt.barriers_path);
  const LineNetwork net = build_line_network(network, opt);
  if (!net.ok()) {
    return false;
  }
  const int src = net.nearest_node(start_x, start_y);
  const ServiceAreaResult area = run_service_area(net, src, cutoff);
  if (!area.ok) {
    return false;
  }
  return write_edges_geojson(output, area);
}

}  // namespace detail
}  // namespace gis
