// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/query/extent_query.h"

#include <algorithm>
#include <vector>

#include "content/browser/camera/gis_host_extent.h"

namespace content {
namespace detail {
namespace {

bool envelope_world(const std::vector<Vertex>& points, content::Extent2* out) {
  if (!out || points.empty()) {
    return false;
  }
  double minx = points[0].x;
  double maxx = minx;
  double miny = points[0].y;
  double maxy = miny;
  for (const Vertex& p : points) {
    minx = std::min(minx, p.x);
    maxx = std::max(maxx, p.x);
    miny = std::min(miny, p.y);
    maxy = std::max(maxy, p.y);
  }
  if (!(maxx > minx)) {
    minx -= 1e-4;
    maxx += 1e-4;
  }
  if (!(maxy > miny)) {
    miny -= 1e-4;
    maxy += 1e-4;
  }
  *out = content::Extent2{minx, -maxy, maxx, -miny};
  return extent_nonempty(*out);
}

}  // namespace

bool compute_extent(const LayerStore& store, double* min_x, double* min_y,
                    double* max_x, double* max_y) {
  if (!min_x || !min_y || !max_x || !max_y) {
    return false;
  }
  bool have = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  for (const GisLayer& layer : store.layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const GisFeature& f : layer.features) {
      for (const Vertex& p : f.points) {
        if (!have) {
          minx = maxx = p.x;
          miny = maxy = p.y;
          have = true;
        } else {
          minx = std::min(minx, p.x);
          miny = std::min(miny, p.y);
          maxx = std::max(maxx, p.x);
          maxy = std::max(maxy, p.y);
        }
      }
    }
  }
  if (!have) {
    return false;
  }
  *min_x = minx;
  *min_y = miny;
  *max_x = maxx;
  *max_y = maxy;
  return true;
}

bool has_china_extent(const LayerStore& store) {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  if (!compute_extent(store, &minx, &miny, &maxx, &maxy)) {
    return false;
  }
  if (minx < 60.0 || maxx > 145.0 || minx > maxx) {
    return false;
  }
  const double south = -maxy;
  const double north = -miny;
  size_t regions = 0;
  size_t lines = 0;
  size_t points = 0;
  size_t texts = 0;
  for (const GisLayer& layer : store.layers()) {
    for (const GisFeature& f : layer.features) {
      switch (f.kind) {
        case GeomKind::kPolygon:
          ++regions;
          break;
        case GeomKind::kLine:
          ++lines;
          break;
        case GeomKind::kPoint:
          ++points;
          break;
        case GeomKind::kText:
          ++texts;
          break;
      }
    }
  }
  // City names may live on point features alone (no parallel text layer).
  const size_t place_marks = points + texts;
  return south >= 3.0 && north <= 60.0 && south < north && regions >= 3 &&
         lines >= 2 && place_marks >= 3;
}

bool active_layer_world_extent(const LayerStore& store,
                               content::Extent2* out) {
  const GisLayer* layer = store.find_layer(store.active_layer_id());
  if (!layer) {
    return false;
  }
  std::vector<Vertex> points;
  for (const GisFeature& feature : layer->features) {
    points.insert(points.end(), feature.points.begin(), feature.points.end());
  }
  return envelope_world(points, out);
}

bool selection_world_extent(const LayerStore& store, content::Extent2* out) {
  const GisFeature* feature = store.selected_feature();
  if (!feature) {
    return false;
  }
  return envelope_world(feature->points, out);
}

content::Extent2 world_extent(const LayerStore& store) {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  if (!compute_extent(store, &minx, &miny, &maxx, &maxy)) {
    return kChinaLonLatExtent;
  }
  return content::Extent2{minx, -maxy, maxx, -miny};
}

void export_land_rings(const LayerStore& store,
                       std::vector<vista::LonLatRing>* out) {
  if (!out) {
    return;
  }
  out->clear();
  auto append_layer = [&](const GisLayer& layer) {
    for (const GisFeature& f : layer.features) {
      if (f.kind != GeomKind::kPolygon || f.points.size() < 3) {
        continue;
      }
      vista::LonLatRing ring;
      ring.x.reserve(f.points.size());
      ring.y.reserve(f.points.size());
      for (const Vertex& p : f.points) {
        ring.x.push_back(p.x);
        ring.y.push_back(-p.y);
      }
      out->push_back(std::move(ring));
    }
  };
  bool used_area = false;
  for (const GisLayer& layer : store.layers()) {
    if (!layer.visible) {
      continue;
    }
    if (layer.name == "area") {
      append_layer(layer);
      used_area = true;
    }
  }
  if (used_area && !out->empty()) {
    return;
  }
  for (const GisLayer& layer : store.layers()) {
    if (!layer.visible) {
      continue;
    }
    if (layer.name == "line" || layer.name == "point" ||
        layer.name == "text") {
      continue;
    }
    append_layer(layer);
  }
}

bool polygon_fit_box(const LayerStore& store, double* min_x, double* min_y,
                     double* max_x, double* max_y) {
  if (!min_x || !min_y || !max_x || !max_y) {
    return false;
  }
  bool have = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  auto accumulate = [&](GeomKind only_kind, bool filter_kind) {
    have = false;
    for (const GisLayer& layer : store.layers()) {
      for (const GisFeature& f : layer.features) {
        if (filter_kind && f.kind != only_kind) {
          continue;
        }
        for (const Vertex& p : f.points) {
          if (!have) {
            minx = maxx = p.x;
            miny = maxy = p.y;
            have = true;
          } else {
            minx = std::min(minx, p.x);
            miny = std::min(miny, p.y);
            maxx = std::max(maxx, p.x);
            maxy = std::max(maxy, p.y);
          }
        }
      }
    }
  };
  accumulate(GeomKind::kPolygon, true);
  if (!have) {
    accumulate(GeomKind::kPolygon, false);
  }
  if (!have) {
    return false;
  }
  *min_x = minx;
  *min_y = miny;
  *max_x = maxx;
  *max_y = maxy;
  return true;
}

}  // namespace detail
}  // namespace content
