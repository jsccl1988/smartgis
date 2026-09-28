// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/edit/feature_edit.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace content {
namespace detail {
namespace {

GeomKind geom_from_tool(const char* tool_id, tool::DraftKind kind) {
  if (tool_id) {
    if (std::strncmp(tool_id, "draw.line", 9) == 0) {
      return GeomKind::kLine;
    }
    if (std::strncmp(tool_id, "draw.poly", 9) == 0 ||
        std::strncmp(tool_id, "draw.rect", 9) == 0) {
      return GeomKind::kPolygon;
    }
  }
  switch (kind) {
    case tool::DraftKind::kLineString:
      return GeomKind::kLine;
    case tool::DraftKind::kPolygon:
    case tool::DraftKind::kRect:
      return GeomKind::kPolygon;
    default:
      return GeomKind::kPoint;
  }
}

double dist2(double ax, double ay, double bx, double by) {
  const double dx = ax - bx;
  const double dy = ay - by;
  return dx * dx + dy * dy;
}

}  // namespace

content::FeatureId append_from_draft(
    LayerStore* store, const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map) {
  if (!store || draft.points.empty()) {
    return {};
  }
  store->ensure_active_layer_or_front();
  MapLayer* layer = store->find_layer(store->active_layer_id());
  if (!layer) {
    return {};
  }
  MapFeature f;
  f.id = store->next_feature_id();
  f.kind = geom_from_tool(tool_id, draft.kind);
  f.points.reserve(draft.points.size());
  for (const tool::DraftPoint& p : draft.points) {
    double mx = static_cast<double>(p.x_px);
    double my = static_cast<double>(p.y_px);
    if (to_map) {
      to_map(p.x_px, p.y_px, &mx, &my);
    }
    f.points.push_back({mx, my});
  }
  if (f.kind == GeomKind::kPolygon && f.points.size() >= 3) {
    if (f.points.front().x != f.points.back().x ||
        f.points.front().y != f.points.back().y) {
      f.points.push_back(f.points.front());
    }
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%u",
                static_cast<unsigned>(store->next_id_value() - 1));
  f.fields = {{"name", std::string(layer->name) + "-" + buf},
              {"type", f.kind == GeomKind::kLine
                           ? "line"
                           : (f.kind == GeomKind::kPolygon ? "polygon"
                                                           : "point")}};
  layer->features.push_back(std::move(f));
  return layer->features.back().id;
}

content::FeatureId move_selected_vertex(LayerStore* store, double map_x,
                                        double map_y, double tol_map) {
  if (!store) {
    return {};
  }
  MapFeature* feature = store->find_feature(store->selected_id());
  if (!feature || feature->points.empty()) {
    return {};
  }
  const double tol2 = tol_map * tol_map;
  int best = -1;
  double best_d = tol2;
  for (size_t i = 0; i < feature->points.size(); ++i) {
    const double d =
        dist2(map_x, map_y, feature->points[i].x, feature->points[i].y);
    if (d <= best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  if (best < 0) {
    return {};
  }
  feature->points[static_cast<size_t>(best)] = {map_x, map_y};
  return feature->id;
}

bool copy_feature_xy(const LayerStore& store, const content::FeatureId& id,
                     std::vector<std::pair<double, double>>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  for (const MapLayer& layer : store.layers()) {
    for (const MapFeature& feature : layer.features) {
      if (!feature_id_eq(feature.id, id)) {
        continue;
      }
      out->reserve(feature.points.size());
      for (const Vertex& p : feature.points) {
        out->push_back({p.x, p.y});
      }
      return true;
    }
  }
  return false;
}

bool add_triangle_layer(LayerStore* store, const std::string& name,
                        const double* xyz, int point_count,
                        const int* triangles, int triangle_count) {
  if (!store || name.empty() || !xyz || point_count < 3 || !triangles ||
      triangle_count < 1) {
    return false;
  }
  if (!store->create_layer(name)) {
    return false;
  }
  MapLayer* layer = store->find_layer(store->active_layer_id());
  if (!layer) {
    return false;
  }
  int added = 0;
  for (int t = 0; t < triangle_count; ++t) {
    const int a = triangles[t * 3];
    const int b = triangles[t * 3 + 1];
    const int c = triangles[t * 3 + 2];
    if (a < 0 || b < 0 || c < 0 || a >= point_count || b >= point_count ||
        c >= point_count) {
      continue;
    }
    MapFeature feature;
    feature.id = store->next_feature_id();
    feature.kind = GeomKind::kPolygon;
    const int idx[3] = {a, b, c};
    for (int k = 0; k < 3; ++k) {
      const int i = idx[k];
      feature.points.push_back({xyz[i * 3], xyz[i * 3 + 1]});
    }
    feature.points.push_back(feature.points.front());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", added + 1);
    feature.fields = {{"name", name + "-" + buf},
                      {"type", "polygon"},
                      {"z", std::to_string(xyz[a * 3 + 2])}};
    layer->features.push_back(std::move(feature));
    ++added;
  }
  if (added == 0) {
    store->remove_layer(layer->id);
    return false;
  }
  return true;
}

const MapFeature* hit_test(LayerStore* store, double map_x, double map_y,
                           double tol_map) {
  if (!store) {
    return nullptr;
  }
  store->clear_selection();
  const double tol2 = tol_map * tol_map;
  MapFeature* best = nullptr;
  double best_d = tol2;
  auto& layers = store->layers();
  for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    if (!it->visible) {
      continue;
    }
    for (auto fit = it->features.rbegin(); fit != it->features.rend(); ++fit) {
      if (fit->points.empty()) {
        continue;
      }
      if (fit->kind == GeomKind::kPoint || fit->kind == GeomKind::kText) {
        const double d =
            dist2(map_x, map_y, fit->points[0].x, fit->points[0].y);
        if (d <= best_d) {
          best_d = d;
          best = &(*fit);
        }
      } else {
        for (const Vertex& p : fit->points) {
          const double d = dist2(map_x, map_y, p.x, p.y);
          if (d <= best_d) {
            best_d = d;
            best = &(*fit);
          }
        }
      }
    }
  }
  if (best) {
    best->selected = true;
    store->set_selected_id(best->id);
  }
  return best;
}

}  // namespace detail
}  // namespace content
