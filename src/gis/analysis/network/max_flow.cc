// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/max_flow.h"

#include <algorithm>
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

struct ResidualEdge {
  int to = -1;
  double cap = 0.0;
  int rev = -1;
  int origin = -1;  // original network edge index, or -1 for reverse
};

bool write_flow_geojson(std::string_view output_path, const MaxFlowResult& r,
                        const LineNetwork& net) {
  if (!r.ok || output_path.empty()) {
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
      ds->CreateLayer("max_flow", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  OGRFieldDefn flow_f("flow", OFTReal);
  OGRFieldDefn total_f("total_flow", OFTReal);
  layer->CreateField(&flow_f);
  layer->CreateField(&total_f);
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  for (size_t i = 0; i < r.used_edges.size(); ++i) {
    const int ei = r.used_edges[i];
    if (ei < 0 || static_cast<size_t>(ei) >= net.edges.size()) {
      continue;
    }
    const NetworkEdge& e = net.edges[static_cast<size_t>(ei)];
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    if (!feat) {
      return false;
    }
    OGRLineString line;
    line.addPoint(net.nodes[static_cast<size_t>(e.from)].first,
                  net.nodes[static_cast<size_t>(e.from)].second);
    line.addPoint(net.nodes[static_cast<size_t>(e.to)].first,
                  net.nodes[static_cast<size_t>(e.to)].second);
    feat->SetGeometry(&line);
    feat->SetField("flow", r.edge_flow[i]);
    feat->SetField("total_flow", r.flow);
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

}  // namespace

MaxFlowResult run_max_flow(const LineNetwork& net, int source, int sink) {
  MaxFlowResult out;
  if (source < 0 || sink < 0 || source == sink ||
      static_cast<size_t>(source) >= net.nodes.size() ||
      static_cast<size_t>(sink) >= net.nodes.size()) {
    out.error = "bad_terminals";
    return out;
  }
  if (net.is_node_blocked(source) || net.is_node_blocked(sink)) {
    out.error = "blocked";
    return out;
  }

  const size_t n = net.nodes.size();
  std::vector<std::vector<ResidualEdge>> g(n);
  auto add_res = [&](int a, int b, double cap, int origin) {
    ResidualEdge fwd{b, cap, static_cast<int>(g[static_cast<size_t>(b)].size()),
                     origin};
    ResidualEdge rev{a, 0.0, static_cast<int>(g[static_cast<size_t>(a)].size()),
                     -1};
    g[static_cast<size_t>(a)].push_back(fwd);
    g[static_cast<size_t>(b)].push_back(rev);
  };

  for (size_t ei = 0; ei < net.edges.size(); ++ei) {
    if (net.is_edge_blocked(static_cast<int>(ei))) {
      continue;
    }
    const NetworkEdge& e = net.edges[ei];
    if (net.is_node_blocked(e.from) || net.is_node_blocked(e.to)) {
      continue;
    }
    if (!(e.capacity > 0.0)) {
      continue;
    }
    add_res(e.from, e.to, e.capacity, static_cast<int>(ei));
  }

  std::vector<double> edge_flow(net.edges.size(), 0.0);
  double total = 0.0;

  while (true) {
    std::vector<int> parent_node(n, -1);
    std::vector<int> parent_edge(n, -1);
    std::queue<int> q;
    q.push(source);
    parent_node[static_cast<size_t>(source)] = source;
    while (!q.empty() && parent_node[static_cast<size_t>(sink)] < 0) {
      const int u = q.front();
      q.pop();
      for (size_t i = 0; i < g[static_cast<size_t>(u)].size(); ++i) {
        const ResidualEdge& e = g[static_cast<size_t>(u)][i];
        if (e.cap <= 1e-15) {
          continue;
        }
        if (parent_node[static_cast<size_t>(e.to)] >= 0) {
          continue;
        }
        parent_node[static_cast<size_t>(e.to)] = u;
        parent_edge[static_cast<size_t>(e.to)] = static_cast<int>(i);
        q.push(e.to);
      }
    }
    if (parent_node[static_cast<size_t>(sink)] < 0) {
      break;
    }

    double aug = std::numeric_limits<double>::infinity();
    for (int v = sink; v != source; v = parent_node[static_cast<size_t>(v)]) {
      const int u = parent_node[static_cast<size_t>(v)];
      const int ei = parent_edge[static_cast<size_t>(v)];
      aug = std::min(aug, g[static_cast<size_t>(u)][static_cast<size_t>(ei)].cap);
    }
    if (!(aug > 0.0) || !std::isfinite(aug)) {
      break;
    }
    for (int v = sink; v != source; v = parent_node[static_cast<size_t>(v)]) {
      const int u = parent_node[static_cast<size_t>(v)];
      const int ei = parent_edge[static_cast<size_t>(v)];
      ResidualEdge& fwd = g[static_cast<size_t>(u)][static_cast<size_t>(ei)];
      fwd.cap -= aug;
      g[static_cast<size_t>(fwd.to)][static_cast<size_t>(fwd.rev)].cap += aug;
      if (fwd.origin >= 0) {
        edge_flow[static_cast<size_t>(fwd.origin)] += aug;
      } else {
        const ResidualEdge& rev =
            g[static_cast<size_t>(fwd.to)][static_cast<size_t>(fwd.rev)];
        if (rev.origin >= 0) {
          edge_flow[static_cast<size_t>(rev.origin)] =
              std::max(0.0, edge_flow[static_cast<size_t>(rev.origin)] - aug);
        }
      }
    }
    total += aug;
  }

  out.flow = total;
  for (size_t ei = 0; ei < edge_flow.size(); ++ei) {
    if (edge_flow[ei] > 1e-12) {
      out.used_edges.push_back(static_cast<int>(ei));
      out.edge_flow.push_back(edge_flow[ei]);
    }
  }
  out.ok = true;
  return out;
}

bool run_max_flow_op(std::string_view args_json) {
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
  op_arg_string(args, "capacity_field", &opt.capacity_field);
  op_arg_string(args, "oneway_field", &opt.oneway_field);
  op_arg_string(args, "turns_path", &opt.turns_path);
  op_arg_string(args, "barriers_path", &opt.barriers_path);
  const LineNetwork net = build_line_network(network, opt);
  if (!net.ok()) {
    return false;
  }
  const int src = net.nearest_node(start_x, start_y);
  const int sink = net.nearest_node(end_x, end_y);
  const MaxFlowResult r = run_max_flow(net, src, sink);
  if (!r.ok) {
    return false;
  }
  return write_flow_geojson(output, r, net);
}

}  // namespace detail
}  // namespace gis
