// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_traffic.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "content/browser/document/map_scene.h"
#include "gis/present/style/style_document.h"
#include "plugin/product/traffic/commands.h"
#include "tool/draft/draft.h"

#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

constexpr const char* kTrafficStyleJson = R"json({
  "version": 8,
  "name": "traffic_path",
  "layers": [
    {"id":"bg","type":"background",
     "paint":{"background-color":"#f5f0e6","background-opacity":1}},
    {"id":"network","type":"line","source-layer":"traffic_network",
     "paint":{"line-color":"#7f8c8d","line-width":2.0}},
    {"id":"path_anim","type":"line","source-layer":"traffic_path_anim",
     "paint":{"line-color":"#3498db","line-width":4.0,"line-opacity":0.55}},
    {"id":"path_final","type":"line","source-layer":"traffic_path",
     "paint":{"line-color":"#e74c3c","line-width":6.0}}
  ]
})json";

bool load_network_lines_layer(content::MapScene* doc, const char* network_path) {
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
      auto append_line = [&](OGRLineString* line) {
        if (!line || line->getNumPoints() < 2) {
          return;
        }
        std::vector<std::pair<double, double>> xy;
        xy.reserve(static_cast<size_t>(line->getNumPoints()));
        for (int i = 0; i < line->getNumPoints(); ++i) {
          xy.emplace_back(line->getX(i), line->getY(i));
        }
        if (append_map_polyline(doc, xy, nullptr)) {
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
  return drew > 0;
}

}  // namespace

// Progressive path: session stores full polyline; anim layer shows prefix.
bool commit_traffic_path(content::MapScene* doc,
                         BrowserUiDelegate* ui,
                         AnalysisPlayback* session,
                         const double* xy,
                         int point_count,
                         double total_cost,
                         int frames,
                         const char* network_path) {
  if (!doc || !xy || point_count < 2 || !session) {
    return false;
  }
  std::vector<double> interleaved(
      xy, xy + static_cast<size_t>(point_count) * 2);
  session->begin_traffic(std::move(interleaved), total_cost, frames,
                         network_path ? network_path : "");
  if (network_path && *network_path) {
    session->set_paths(network_path, session->output_path());
  }

  doc->clear();
  if (!apply_style_json(doc, kTrafficStyleJson)) {
    return false;
  }

  if (network_path && *network_path) {
    if (!load_network_lines_layer(doc, network_path)) {
      return false;
    }
  }

  std::vector<std::pair<double, double>> pts;
  pts.reserve(static_cast<size_t>(point_count));
  for (int i = 0; i < point_count; ++i) {
    pts.emplace_back(xy[static_cast<size_t>(i) * 2],
                     xy[static_cast<size_t>(i) * 2 + 1]);
  }

  const int end = session->traffic_prefix_point_count();
  if (!doc->create_layer("traffic_path_anim", "LineString")) {
    return false;
  }
  std::vector<std::pair<double, double>> prefix(
      pts.begin(), pts.begin() + static_cast<size_t>(end));
  if (!append_map_polyline(doc, prefix, "anim")) {
    return false;
  }

  if (!doc->create_layer("traffic_path", "LineString")) {
    return false;
  }
  if (!append_map_polyline(doc, pts, nullptr)) {
    return false;
  }

  double cx = 0;
  double cy = 0;
  for (const auto& p : pts) {
    cx += p.first;
    cy += p.second;
  }
  cx /= static_cast<double>(pts.size());
  cy /= static_cast<double>(pts.size());
  add_standin_mesh(doc, ui, "Traffic path 3D", cx, -cy, 0.008);
  return refresh_ui_after_layer(ui);
}

void wire_traffic_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_traffic_path_writer(
      [browser](const double* xy, int point_count, double total_cost, int frames,
             const char* network_path) {
        return commit_traffic_path(&browser->session().document(), browser->ui(),
                                   &browser->analysis_playback(), xy, point_count,
                                   total_cost, frames, network_path);
      });
}

}  // namespace detail
}  // namespace app
