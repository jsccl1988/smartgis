// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/cost_path.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <rapidjson/document.h>

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

constexpr double kSnap = 1e-6;

NodeKey make_key(double x, double y) {
  return {static_cast<long long>(std::llround(x / kSnap)),
          static_cast<long long>(std::llround(y / kSnap))};
}

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

void add_line_edges(OGRLineString* line,
                    OGRFeature* feat,
                    const std::string& weight_field,
                    std::unordered_map<NodeKey, int, NodeKeyHash>* index,
                    std::vector<std::pair<double, double>>* coords,
                    std::vector<std::vector<std::pair<int, double>>>* adj) {
  if (!line || line->getNumPoints() < 2 || !index || !coords || !adj) {
    return;
  }
  const bool use_field = !weight_field.empty() && feat &&
                         feat->GetFieldIndex(weight_field.c_str()) >= 0;
  const double field_w =
      use_field ? feat->GetFieldAsDouble(weight_field.c_str()) : -1.0;

  auto ensure_node = [&](double x, double y) -> int {
    const NodeKey key = make_key(x, y);
    const auto it = index->find(key);
    if (it != index->end()) {
      return it->second;
    }
    const int id = static_cast<int>(coords->size());
    coords->push_back({x, y});
    adj->emplace_back();
    (*index)[key] = id;
    return id;
  };

  for (int i = 0; i + 1 < line->getNumPoints(); ++i) {
    const double x0 = line->getX(i);
    const double y0 = line->getY(i);
    const double x1 = line->getX(i + 1);
    const double y1 = line->getY(i + 1);
    const int a = ensure_node(x0, y0);
    const int b = ensure_node(x1, y1);
    if (a == b) {
      continue;
    }
    const double len =
        (Eigen::Vector2d(x1 - x0, y1 - y0)).norm();
    // weight_field is an impedance multiplier (time/cost per meter); omit → length.
    const double w = (field_w > 0.0) ? (len * field_w) : len;
    if (!(w > 0.0)) {
      continue;
    }
    (*adj)[static_cast<size_t>(a)].push_back({b, w});
    (*adj)[static_cast<size_t>(b)].push_back({a, w});
  }
}

void collect_geometry(OGRGeometry* geom,
                      OGRFeature* feat,
                      const std::string& weight_field,
                      std::unordered_map<NodeKey, int, NodeKeyHash>* index,
                      std::vector<std::pair<double, double>>* coords,
                      std::vector<std::vector<std::pair<int, double>>>* adj) {
  if (!geom) {
    return;
  }
  const OGRwkbGeometryType t =
      wkbFlatten(geom->getGeometryType());
  if (t == wkbLineString) {
    add_line_edges(geom->toLineString(), feat, weight_field, index, coords,
                   adj);
    return;
  }
  if (t == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    for (int i = 0; i < multi->getNumGeometries(); ++i) {
      add_line_edges(multi->getGeometryRef(i)->toLineString(), feat,
                     weight_field, index, coords, adj);
    }
  }
}

int nearest_node(const std::vector<std::pair<double, double>>& coords,
                 double x,
                 double y) {
  int best = -1;
  double best_d2 = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < coords.size(); ++i) {
    const double dx = coords[i].first - x;
    const double dy = coords[i].second - y;
    const double d2 = dx * dx + dy * dy;
    if (d2 < best_d2) {
      best_d2 = d2;
      best = static_cast<int>(i);
    }
  }
  return best;
}

// Queue key: least cost, then most rightward (China drive-on-right).
struct QueueItem {
  double cost = 0.0;
  double neg_rightness = 0.0;
  int node = -1;

  bool operator>(const QueueItem& o) const {
    if (cost != o.cost) {
      return cost > o.cost;
    }
    if (neg_rightness != o.neg_rightness) {
      return neg_rightness > o.neg_rightness;
    }
    return node > o.node;
  }
};

// Near-equal band so China RHT rightmost can win over tiny geometric wiggles.
bool cost_tie(double nd, double cur) {
  constexpr double kAbs = 1e-12;
  constexpr double kRel = 1e-3;
  const double scale = std::max(1.0, std::max(std::abs(nd), std::abs(cur)));
  return std::abs(nd - cur) <= std::max(kAbs, kRel * scale);
}

bool cost_better(double nd, double cur) {
  return !cost_tie(nd, cur) && nd < cur;
}

}  // namespace

CostPathResult run_cost_path(std::string_view network_path,
                             double start_x,
                             double start_y,
                             double end_x,
                             double end_y,
                             std::string_view weight_field) {
  CostPathResult out;
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

  std::unordered_map<NodeKey, int, NodeKeyHash> index;
  std::vector<std::pair<double, double>> coords;
  std::vector<std::vector<std::pair<int, double>>> adj;
  const std::string wf(weight_field);

  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* layer = ds->GetLayer(li);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    OGRFeature* feat = nullptr;
    while ((feat = layer->GetNextFeature()) != nullptr) {
      collect_geometry(feat->GetGeometryRef(), feat, wf, &index, &coords,
                       &adj);
      OGRFeature::DestroyFeature(feat);
    }
  }

  if (coords.size() < 2) {
    out.error = "no_edges";
    return out;
  }

  const int src = nearest_node(coords, start_x, start_y);
  const int dst = nearest_node(coords, end_x, end_y);
  if (src < 0 || dst < 0) {
    out.error = "no_nodes";
    return out;
  }
  if (src == dst) {
    out.ok = true;
    out.xy = {coords[static_cast<size_t>(src)].first,
              coords[static_cast<size_t>(src)].second};
    return out;
  }

  const size_t n = coords.size();
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  std::vector<double> rightness(n, -std::numeric_limits<double>::infinity());
  std::vector<int> prev(n, -1);

  // Traveler facing start→end: clockwise normal is the right side (China RHT).
  const double travel_dx = end_x - start_x;
  const double travel_dy = end_y - start_y;
  const double right_x = travel_dy;
  const double right_y = -travel_dx;
  const double right_norm = std::hypot(right_x, right_y);
  const double inv_right = right_norm > 1e-15 ? (1.0 / right_norm) : 0.0;

  auto edge_rightness = [&](int a, int b) -> double {
    const auto& ca = coords[static_cast<size_t>(a)];
    const auto& cb = coords[static_cast<size_t>(b)];
    const double mx = 0.5 * (ca.first + cb.first) - start_x;
    const double my = 0.5 * (ca.second + cb.second) - start_y;
    return (mx * right_x + my * right_y) * inv_right;
  };

  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq;
  dist[static_cast<size_t>(src)] = 0.0;
  rightness[static_cast<size_t>(src)] = 0.0;
  pq.push({0.0, 0.0, src});

  while (!pq.empty()) {
    // Keep scanning near-equal (China RHT) candidates after dst is reached.
    if (std::isfinite(dist[static_cast<size_t>(dst)]) &&
        cost_better(dist[static_cast<size_t>(dst)], pq.top().cost)) {
      break;
    }
    const QueueItem top = pq.top();
    pq.pop();
    const double d = top.cost;
    const int u = top.node;
    if (cost_better(dist[static_cast<size_t>(u)], d) ||
        (cost_tie(dist[static_cast<size_t>(u)], d) &&
         top.neg_rightness > -rightness[static_cast<size_t>(u)] + 1e-15)) {
      continue;
    }
    for (const auto& [v, w] : adj[static_cast<size_t>(u)]) {
      const double nd = d + w;
      const double nr =
          rightness[static_cast<size_t>(u)] + edge_rightness(u, v);
      const double& cur_d = dist[static_cast<size_t>(v)];
      const double& cur_r = rightness[static_cast<size_t>(v)];
      if (cost_better(nd, cur_d) ||
          (cost_tie(nd, cur_d) && nr > cur_r + 1e-15)) {
        dist[static_cast<size_t>(v)] = nd;
        rightness[static_cast<size_t>(v)] = nr;
        prev[static_cast<size_t>(v)] = u;
        pq.push({nd, -nr, v});
      }
    }
  }

  if (!std::isfinite(dist[static_cast<size_t>(dst)])) {
    out.error = "unreachable";
    return out;
  }

  std::vector<int> chain;
  for (int cur = dst; cur >= 0; cur = prev[static_cast<size_t>(cur)]) {
    chain.push_back(cur);
    if (cur == src) {
      break;
    }
  }
  if (chain.empty() || chain.back() != src) {
    out.error = "reconstruct_failed";
    return out;
  }
  std::reverse(chain.begin(), chain.end());
  out.xy.reserve(chain.size() * 2);
  for (int id : chain) {
    out.xy.push_back(coords[static_cast<size_t>(id)].first);
    out.xy.push_back(coords[static_cast<size_t>(id)].second);
  }
  out.total_cost = dist[static_cast<size_t>(dst)];
  out.ok = true;
  return out;
}

bool write_path_geojson(std::string_view output_path,
                        const CostPathResult& path) {
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
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string network;
  std::string output;
  if (!json_get_string(args, "network", &network) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  double start_x = 0;
  double start_y = 0;
  double end_x = 0;
  double end_y = 0;
  if (!json_get_double(args, "start_x", &start_x) ||
      !json_get_double(args, "start_y", &start_y) ||
      !json_get_double(args, "end_x", &end_x) ||
      !json_get_double(args, "end_y", &end_y)) {
    return false;
  }
  std::string weight_field;
  json_get_string(args, "weight_field", &weight_field);
  const CostPathResult path =
      run_cost_path(network, start_x, start_y, end_x, end_y, weight_field);
  if (!path.ok) {
    return false;
  }
  return write_path_geojson(output, path);
}

}  // namespace detail
}  // namespace gis
