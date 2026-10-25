// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/apsp.h"

#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/ops/runner/ops_args.h"
#include "ogr_api.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"

#include <rapidjson/document.h>

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

std::vector<double> dijkstra_dist(const LineNetwork& net, int source) {
  const size_t n = net.nodes.size();
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  if (source < 0 || static_cast<size_t>(source) >= n ||
      net.is_node_blocked(source)) {
    return dist;
  }
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq;
  dist[static_cast<size_t>(source)] = 0.0;
  pq.push({0.0, source, -1});
  while (!pq.empty()) {
    const QueueItem top = pq.top();
    pq.pop();
    if (top.cost > dist[static_cast<size_t>(top.node)] + 1e-15) {
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
      if (nd < dist[static_cast<size_t>(e.to)]) {
        dist[static_cast<size_t>(e.to)] = nd;
        pq.push({nd, e.to, top.node});
      }
    }
  }
  return dist;
}

bool write_apsp_geojson(std::string_view output_path, const ApspResult& r,
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
  OGRLayer* layer = ds->CreateLayer("apsp", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  OGRFieldDefn from_f("from", OFTInteger);
  OGRFieldDefn to_f("to", OFTInteger);
  OGRFieldDefn cost_f("cost", OFTReal);
  layer->CreateField(&from_f);
  layer->CreateField(&to_f);
  layer->CreateField(&cost_f);
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  for (const ApspPair& p : r.pairs) {
    if (!p.reachable || p.from < 0 || p.to < 0) {
      continue;
    }
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    if (!feat) {
      return false;
    }
    OGRLineString line;
    line.addPoint(net.nodes[static_cast<size_t>(p.from)].first,
                  net.nodes[static_cast<size_t>(p.from)].second);
    line.addPoint(net.nodes[static_cast<size_t>(p.to)].first,
                  net.nodes[static_cast<size_t>(p.to)].second);
    feat->SetGeometry(&line);
    feat->SetField("from", p.from);
    feat->SetField("to", p.to);
    feat->SetField("cost", p.cost);
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

}  // namespace

ApspResult run_apsp(const LineNetwork& net, const std::vector<int>& sources) {
  ApspResult out;
  if (sources.empty()) {
    out.error = "no_sources";
    return out;
  }
  for (int s : sources) {
    if (s < 0 || static_cast<size_t>(s) >= net.nodes.size()) {
      out.error = "bad_source";
      return out;
    }
  }
  out.pairs.reserve(sources.size() * sources.size());
  for (int s : sources) {
    const std::vector<double> dist = dijkstra_dist(net, s);
    for (int t : sources) {
      ApspPair pair;
      pair.from = s;
      pair.to = t;
      if (s == t) {
        pair.reachable = true;
        pair.cost = 0.0;
      } else if (std::isfinite(dist[static_cast<size_t>(t)])) {
        pair.reachable = true;
        pair.cost = dist[static_cast<size_t>(t)];
      }
      out.pairs.push_back(pair);
    }
  }
  out.ok = true;
  return out;
}

bool run_apsp_op(std::string_view args_json) {
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
  NetworkBuildOptions opt;
  op_arg_string(args, "weight_field", &opt.weight_field);
  op_arg_string(args, "oneway_field", &opt.oneway_field);
  op_arg_string(args, "turns_path", &opt.turns_path);
  op_arg_string(args, "barriers_path", &opt.barriers_path);
  const LineNetwork net = build_line_network(network, opt);
  if (!net.ok()) {
    return false;
  }

  std::vector<int> sources;
  const auto it = args.FindMember("sources");
  if (it != args.MemberEnd() && it->value.IsArray()) {
    for (rapidjson::SizeType i = 0; i < it->value.Size(); ++i) {
      const auto& pt = it->value[i];
      if (!pt.IsObject()) {
        return false;
      }
      double x = 0;
      double y = 0;
      if (!op_arg_double(pt, "x", &x) || !op_arg_double(pt, "y", &y)) {
        return false;
      }
      const int n = net.nearest_node(x, y);
      if (n < 0) {
        return false;
      }
      sources.push_back(n);
    }
  } else {
    return false;
  }

  const ApspResult r = run_apsp(net, sources);
  if (!r.ok) {
    return false;
  }
  return write_apsp_geojson(output, r, net);
}

}  // namespace detail
}  // namespace gis
