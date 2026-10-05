// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/present/present.h"

#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"

#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace {

void format_heat01(double t01, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return;
  }
  std::snprintf(buf, cap, "%.1f", std::clamp(t01, 0.0, 1.0) * 100.0);
}

bool paint_traffic_blocks(content::GisDocument* doc, double min_x, double min_y,
                          double max_x, double max_y) {
  if (!doc || !(max_x > min_x) || !(max_y > min_y)) {
    return false;
  }
  if (!doc->create_layer("traffic_blocks", "Polygon")) {
    return false;
  }
  constexpr int kN = 10;
  const double dx = (max_x - min_x) / static_cast<double>(kN);
  const double dy = (max_y - min_y) / static_cast<double>(kN);
  for (int j = 0; j < kN; ++j) {
    for (int i = 0; i < kN; ++i) {
      const double x0 = min_x + dx * i;
      const double y0 = min_y + dy * j;
      const double x1 = x0 + dx;
      const double y1 = y0 + dy;
      const std::vector<std::pair<double, double>> ring = {
          {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
      };
      const double heat_t = 0.2 + 0.5 * (static_cast<double>(i) / (kN - 1)) +
                            0.3 * (static_cast<double>((i + j) & 1));
      char heat_buf[32] = {};
      format_heat01(heat_t, heat_buf, sizeof(heat_buf));
      if (!append_map_polygon(doc, ring, heat_buf)) {
        return false;
      }
    }
  }
  return true;
}

bool load_network_lines_layer(content::GisDocument* doc, const char* network_path,
                              double* out_min_x, double* out_min_y,
                              double* out_max_x, double* out_max_y) {
  if (!doc || !network_path || !*network_path) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      network_path, GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr)));
  if (!ds) {
    return false;
  }
  if (!doc->create_layer("traffic_network", "LineString")) {
    return false;
  }
  int drew = 0;
  bool have_env = false;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
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
      double cost = 0.55;
      const int cost_i = feat->GetFieldIndex("cost");
      if (cost_i >= 0) {
        cost = feat->GetFieldAsDouble(cost_i);
      }
      const char* klass = "collector";
      const int class_i = feat->GetFieldIndex("class");
      if (class_i >= 0) {
        const char* c = feat->GetFieldAsString(class_i);
        if (c && c[0]) {
          klass = c;
        }
      }
      char heat_buf[32] = {};
      format_heat01(std::clamp(cost, 0.05, 1.5) / 1.5, heat_buf,
                    sizeof(heat_buf));

      auto append_line = [&](OGRLineString* line) {
        if (!line || line->getNumPoints() < 2) {
          return;
        }
        std::vector<std::pair<double, double>> xy;
        xy.reserve(static_cast<size_t>(line->getNumPoints()));
        for (int i = 0; i < line->getNumPoints(); ++i) {
          const double x = line->getX(i);
          const double y = line->getY(i);
          xy.emplace_back(x, y);
          if (!have_env) {
            min_x = max_x = x;
            min_y = max_y = y;
            have_env = true;
          } else {
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
          }
        }
        if (append_map_polyline(doc, xy, nullptr, klass, heat_buf)) {
          ++drew;
        }
      };
      const OGRwkbGeometryType t = wkbFlatten(geom->getGeometryType());
      if (t == wkbLineString) {
        append_line(geom->toLineString());
      } else if (t == wkbMultiLineString) {
        auto* multi = geom->toMultiLineString();
        for (int i = 0; i < multi->getNumGeometries(); ++i) {
          append_line(multi->getGeometryRef(i)->toLineString());
        }
      }
      OGRFeature::DestroyFeature(feat);
    }
  }
  if (have_env && out_min_x && out_min_y && out_max_x && out_max_y) {
    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
  }
  return drew > 0;
}

}  // namespace

bool present_traffic_path(content::GisDocument* doc,
                          const double* xy,
                          int point_count,
                          double total_cost,
                          const char* network_path,
                          int anim_prefix_points) {
  if (!doc || !xy || point_count < 2) {
    return false;
  }

  doc->remove_layer("traffic_blocks");
  doc->remove_layer("traffic_network");
  doc->remove_layer("traffic_path");
  doc->remove_layer("traffic_path_anim");
  if (!apply_style_resource(doc, "smartgis.traffic", "traffic.style.json")) {
    return false;
  }

  double min_x = xy[0];
  double max_x = xy[0];
  double min_y = xy[1];
  double max_y = xy[1];
  for (int i = 0; i < point_count; ++i) {
    const double x = xy[static_cast<size_t>(i) * 2];
    const double y = xy[static_cast<size_t>(i) * 2 + 1];
    min_x = std::min(min_x, x);
    max_x = std::max(max_x, x);
    min_y = std::min(min_y, y);
    max_y = std::max(max_y, y);
  }
  min_x = std::min(min_x, 116.335);
  max_x = std::max(max_x, 116.452);
  min_y = std::min(min_y, 39.870);
  max_y = std::max(max_y, 39.9285);

  const double pad_x = std::max(0.002, (max_x - min_x) * 0.04);
  const double pad_y = std::max(0.002, (max_y - min_y) * 0.04);
  if (!paint_traffic_blocks(doc, min_x - pad_x, min_y - pad_y, max_x + pad_x,
                            max_y + pad_y)) {
    return false;
  }

  if (network_path && *network_path) {
    double nmin_x = 0;
    double nmin_y = 0;
    double nmax_x = 0;
    double nmax_y = 0;
    if (!load_network_lines_layer(doc, network_path, &nmin_x, &nmin_y, &nmax_x,
                                  &nmax_y)) {
      return false;
    }
    (void)nmin_x;
    (void)nmin_y;
    (void)nmax_x;
    (void)nmax_y;
  }

  std::vector<std::pair<double, double>> pts;
  pts.reserve(static_cast<size_t>(point_count));
  for (int i = 0; i < point_count; ++i) {
    pts.emplace_back(xy[static_cast<size_t>(i) * 2],
                     xy[static_cast<size_t>(i) * 2 + 1]);
  }

  if (!doc->create_layer("traffic_path", "LineString")) {
    return false;
  }
  if (!append_map_polyline(doc, pts, nullptr, "route", "100")) {
    return false;
  }

  if (!doc->create_layer("traffic_path_anim", "LineString")) {
    return false;
  }
  const int prefix_n = std::max(2, anim_prefix_points);
  std::vector<std::pair<double, double>> prefix(
      pts.begin(),
      pts.begin() + static_cast<size_t>(std::min(prefix_n, point_count)));
  if (!append_map_polyline(doc, prefix, "anim", "anim", "80")) {
    return false;
  }

  (void)total_cost;
  return true;
}

}  // namespace plugin
