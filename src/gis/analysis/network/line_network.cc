// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/line_network.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace detail {
namespace {

struct NodeKey {
  long long x = 0;
  long long y = 0;
  bool operator==(const NodeKey& o) const { return x == o.x && y == o.y; }
};

struct NodeKeyHash {
  size_t operator()(const NodeKey& k) const {
    return (static_cast<size_t>(k.x) * 1315423911u) ^
           static_cast<size_t>(k.y);
  }
};

struct TurnKey {
  int from = -1;
  int via = -1;
  int to = -1;
  bool operator==(const TurnKey& o) const {
    return from == o.from && via == o.via && to == o.to;
  }
};

struct TurnKeyHash {
  size_t operator()(const TurnKey& k) const {
    size_t h = static_cast<size_t>(k.from);
    h = h * 1315423911u + static_cast<size_t>(k.via);
    h = h * 1315423911u + static_cast<size_t>(k.to);
    return h;
  }
};

NodeKey make_key(double x, double y, double snap) {
  return {static_cast<long long>(std::llround(x / snap)),
          static_cast<long long>(std::llround(y / snap))};
}

std::string upper_ascii(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

// 1 = forward only, -1 = reverse only, 0 = both.
int parse_oneway(OGRFeature* feat, const std::string& field) {
  if (!feat || field.empty()) {
    return 0;
  }
  const int idx = feat->GetFieldIndex(field.c_str());
  if (idx < 0) {
    return 0;
  }
  if (feat->IsFieldNull(idx)) {
    return 0;
  }
  const OGRFieldType ft = feat->GetFieldDefnRef(idx)->GetType();
  if (ft == OFTInteger || ft == OFTInteger64 || ft == OFTReal) {
    const double v = feat->GetFieldAsDouble(idx);
    if (v > 0.0) {
      return 1;
    }
    if (v < 0.0) {
      return -1;
    }
    return 0;
  }
  const std::string raw = upper_ascii(feat->GetFieldAsString(idx));
  if (raw.empty() || raw == "B" || raw == "NO" || raw == "FALSE" ||
      raw == "0") {
    return 0;
  }
  if (raw == "FT" || raw == "F" || raw == "YES" || raw == "TRUE" ||
      raw == "1") {
    return 1;
  }
  if (raw == "TF" || raw == "T" || raw == "-1") {
    return -1;
  }
  return 0;
}

double field_or(OGRFeature* feat, const std::string& field, double fallback) {
  if (!feat || field.empty()) {
    return fallback;
  }
  const int idx = feat->GetFieldIndex(field.c_str());
  if (idx < 0 || feat->IsFieldNull(idx)) {
    return fallback;
  }
  const double v = feat->GetFieldAsDouble(idx);
  return (v > 0.0) ? v : fallback;
}

class GraphBuilder {
 public:
  explicit GraphBuilder(double snap) : snap_(snap) {}

  int ensure_node(double x, double y) {
    const NodeKey key = make_key(x, y, snap_);
    const auto it = index_.find(key);
    if (it != index_.end()) {
      return it->second;
    }
    const int id = static_cast<int>(nodes_.size());
    nodes_.push_back({x, y});
    out_edges_.emplace_back();
    index_[key] = id;
    return id;
  }

  void add_directed(int a, int b, double cost, double capacity, int fid) {
    if (a < 0 || b < 0 || a == b || !(cost > 0.0)) {
      return;
    }
    const int ei = static_cast<int>(edges_.size());
    edges_.push_back({a, b, cost, capacity > 0.0 ? capacity : 1.0, fid});
    out_edges_[static_cast<size_t>(a)].push_back(ei);
  }

  void add_line(OGRLineString* line, OGRFeature* feat,
                const NetworkBuildOptions& opt) {
    if (!line || line->getNumPoints() < 2) {
      return;
    }
    const double impedance = field_or(feat, opt.weight_field, 1.0);
    const double capacity = field_or(feat, opt.capacity_field, 1.0);
    const int oneway = parse_oneway(feat, opt.oneway_field);
    const int fid = feat ? static_cast<int>(feat->GetFID()) : -1;

    for (int i = 0; i + 1 < line->getNumPoints(); ++i) {
      const double x0 = line->getX(i);
      const double y0 = line->getY(i);
      const double x1 = line->getX(i + 1);
      const double y1 = line->getY(i + 1);
      const int a = ensure_node(x0, y0);
      const int b = ensure_node(x1, y1);
      const double len =
          (Eigen::Vector2d(x1 - x0, y1 - y0)).norm();
      const double cost = len * impedance;
      if (!(cost > 0.0)) {
        continue;
      }
      if (oneway >= 0) {
        add_directed(a, b, cost, capacity, fid);
      }
      if (oneway <= 0) {
        add_directed(b, a, cost, capacity, fid);
      }
    }
  }

  void collect_geom(OGRGeometry* geom, OGRFeature* feat,
                    const NetworkBuildOptions& opt) {
    if (!geom) {
      return;
    }
    const OGRwkbGeometryType t = wkbFlatten(geom->getGeometryType());
    if (t == wkbLineString) {
      add_line(geom->toLineString(), feat, opt);
      return;
    }
    if (t == wkbMultiLineString) {
      auto* multi = geom->toMultiLineString();
      for (int i = 0; i < multi->getNumGeometries(); ++i) {
        add_line(multi->getGeometryRef(i)->toLineString(), feat, opt);
      }
      return;
    }
    if (t == wkbGeometryCollection) {
      auto* col = geom->toGeometryCollection();
      for (int i = 0; i < col->getNumGeometries(); ++i) {
        collect_geom(col->getGeometryRef(i), feat, opt);
      }
    }
  }

  void add_turn_ban(int from, int via, int to) {
    if (from < 0 || via < 0 || to < 0 || from == via || via == to) {
      return;
    }
    const TurnKey key{from, via, to};
    if (!turn_set_.insert(key).second) {
      return;
    }
    turn_bans_.push_back({from, via, to});
  }

  void apply_turn_line(OGRLineString* line) {
    if (!line || line->getNumPoints() < 3) {
      return;
    }
    const int a = ensure_node(line->getX(0), line->getY(0));
    const int b = ensure_node(line->getX(1), line->getY(1));
    const int c = ensure_node(line->getX(line->getNumPoints() - 1),
                              line->getY(line->getNumPoints() - 1));
    add_turn_ban(a, b, c);
  }

  void apply_barrier_point(double x, double y) {
    const int n = nearest(x, y);
    if (n >= 0) {
      ensure_barrier_size();
      node_barrier_[static_cast<size_t>(n)] = 1;
    }
  }

  void apply_barrier_line(OGRLineString* line) {
    if (!line || line->getNumPoints() < 1) {
      return;
    }
    std::vector<int> chain;
    chain.reserve(static_cast<size_t>(line->getNumPoints()));
    for (int i = 0; i < line->getNumPoints(); ++i) {
      const int n = nearest(line->getX(i), line->getY(i));
      if (n >= 0) {
        chain.push_back(n);
        ensure_barrier_size();
        node_barrier_[static_cast<size_t>(n)] = 1;
      }
    }
    ensure_barrier_size();
    for (size_t i = 0; i + 1 < chain.size(); ++i) {
      const int a = chain[i];
      const int b = chain[i + 1];
      for (size_t ei = 0; ei < edges_.size(); ++ei) {
        const NetworkEdge& e = edges_[ei];
        if ((e.from == a && e.to == b) || (e.from == b && e.to == a)) {
          edge_barrier_[ei] = 1;
        }
      }
    }
  }

  int nearest(double x, double y) const {
    int best = -1;
    double best_d2 = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < nodes_.size(); ++i) {
      const double dx = nodes_[i].first - x;
      const double dy = nodes_[i].second - y;
      const double d2 = dx * dx + dy * dy;
      if (d2 < best_d2) {
        best_d2 = d2;
        best = static_cast<int>(i);
      }
    }
    return best;
  }

  void ensure_barrier_size() {
    if (node_barrier_.size() < nodes_.size()) {
      node_barrier_.resize(nodes_.size(), 0);
    }
    if (edge_barrier_.size() < edges_.size()) {
      edge_barrier_.resize(edges_.size(), 0);
    }
  }

  LineNetwork finish() {
    LineNetwork out;
    out.nodes = std::move(nodes_);
    out.edges = std::move(edges_);
    out.out_edges = std::move(out_edges_);
    out.turn_bans = std::move(turn_bans_);
    ensure_barrier_size();
    out.node_barrier = std::move(node_barrier_);
    out.edge_barrier = std::move(edge_barrier_);
    if (out.edges.empty()) {
      out.error = "no_edges";
    }
    return out;
  }

 private:
  double snap_ = 1e-6;
  std::unordered_map<NodeKey, int, NodeKeyHash> index_;
  std::vector<std::pair<double, double>> nodes_;
  std::vector<NetworkEdge> edges_;
  std::vector<std::vector<int>> out_edges_;
  std::vector<TurnBan> turn_bans_;
  std::unordered_set<TurnKey, TurnKeyHash> turn_set_;
  std::vector<uint8_t> node_barrier_;
  std::vector<uint8_t> edge_barrier_;
};

bool load_side_dataset(std::string_view path, GraphBuilder* b,
                       bool turns) {
  if (!b || path.empty()) {
    return true;
  }
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
      nullptr, nullptr)));
  if (!ds) {
    return false;
  }
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* layer = ds->GetLayer(li);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      OGRGeometry* geom = feat->GetGeometryRef();
      if (!geom) {
        OGRFeature::DestroyFeature(feat);
        continue;
      }
      const OGRwkbGeometryType t = wkbFlatten(geom->getGeometryType());
      if (turns) {
        if (t == wkbLineString) {
          b->apply_turn_line(geom->toLineString());
        } else if (t == wkbMultiLineString) {
          auto* multi = geom->toMultiLineString();
          for (int i = 0; i < multi->getNumGeometries(); ++i) {
            b->apply_turn_line(multi->getGeometryRef(i)->toLineString());
          }
        }
      } else {
        if (t == wkbPoint) {
          auto* p = geom->toPoint();
          b->apply_barrier_point(p->getX(), p->getY());
        } else if (t == wkbMultiPoint) {
          auto* mp = geom->toMultiPoint();
          for (int i = 0; i < mp->getNumGeometries(); ++i) {
            auto* p = mp->getGeometryRef(i)->toPoint();
            b->apply_barrier_point(p->getX(), p->getY());
          }
        } else if (t == wkbLineString) {
          b->apply_barrier_line(geom->toLineString());
        } else if (t == wkbMultiLineString) {
          auto* multi = geom->toMultiLineString();
          for (int i = 0; i < multi->getNumGeometries(); ++i) {
            b->apply_barrier_line(multi->getGeometryRef(i)->toLineString());
          }
        }
      }
      OGRFeature::DestroyFeature(feat);
    }
  }
  return true;
}

}  // namespace

int LineNetwork::nearest_node(double x, double y) const {
  int best = -1;
  double best_d2 = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (is_node_blocked(static_cast<int>(i))) {
      continue;
    }
    const double dx = nodes[i].first - x;
    const double dy = nodes[i].second - y;
    const double d2 = dx * dx + dy * dy;
    if (d2 < best_d2) {
      best_d2 = d2;
      best = static_cast<int>(i);
    }
  }
  return best;
}

bool LineNetwork::is_node_blocked(int node) const {
  if (node < 0 || static_cast<size_t>(node) >= node_barrier.size()) {
    return false;
  }
  return node_barrier[static_cast<size_t>(node)] != 0;
}

bool LineNetwork::is_edge_blocked(int edge) const {
  if (edge < 0 || static_cast<size_t>(edge) >= edge_barrier.size()) {
    return false;
  }
  return edge_barrier[static_cast<size_t>(edge)] != 0;
}

bool LineNetwork::is_turn_banned(int from, int via, int to) const {
  for (const TurnBan& ban : turn_bans) {
    if (ban.from == from && ban.via == via && ban.to == to) {
      return true;
    }
  }
  return false;
}

LineNetwork build_line_network(std::string_view network_path,
                               const NetworkBuildOptions& options) {
  LineNetwork out;
  if (network_path.empty()) {
    out.error = "empty_network";
    return out;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      std::string(network_path).c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
      nullptr, nullptr, nullptr)));
  if (!ds) {
    out.error = "open_failed";
    return out;
  }

  GraphBuilder builder(options.snap > 0.0 ? options.snap : 1e-6);
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* layer = ds->GetLayer(li);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      builder.collect_geom(feat->GetGeometryRef(), feat, options);
      OGRFeature::DestroyFeature(feat);
    }
  }

  if (!load_side_dataset(options.turns_path, &builder, true)) {
    out.error = "turns_open_failed";
    return out;
  }
  if (!load_side_dataset(options.barriers_path, &builder, false)) {
    out.error = "barriers_open_failed";
    return out;
  }

  return builder.finish();
}

}  // namespace detail
}  // namespace gis
