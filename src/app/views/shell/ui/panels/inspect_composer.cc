// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/panels/inspect_composer.h"
#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/public/map_contents.h"
#include "gis/carto/style/style_document.h"
#include "gis/carto/style/style_types.h"
#include "tool/draft/draft.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/style/symbology_panel.h"

namespace app {

// Measure / selection / legend / layer-properties construction and sync.

// Measure / selection / legend / layer-properties chrome wire.
InspectComposer::InspectComposer(BrowserView* host) : host_(host) {}

void InspectComposer::wire_measure_panel() {
  if (!host_->measure_panel_) {
    return;
  }
  host_->measure_panel_->set_unit_text("m");
  host_->measure_panel_->set_mode_change([this](ui::views::MeasurePanel::Mode mode) {
    host_->measure_armed_ = true;
    const char* name = "length";
    const char* tool = "edit.append.linestring";
    if (mode == ui::views::MeasurePanel::Mode::kArea) {
      name = "area";
      tool = "edit.append.polygon";
    } else if (mode == ui::views::MeasurePanel::Mode::kAzimuth) {
      name = "azimuth";
      tool = "edit.append.linestring";
    }
    host_->browser_->run_tool_command(tool);
    host_->show_inspector_tab_index(host_->measure_tab_);
    host_->set_status_message(std::string("Measure mode: ") + name +
                       " (digitize; not stored)");
  });
}


void InspectComposer::wire_selection_panel() {
  if (!host_->selection_panel_) {
    return;
  }
  host_->selection_panel_->set_command([this](const std::string& id) {
    if (id == "selection.clear") {
      host_->browser_->run_tool_command("selection.clear");
      host_->sync_selection_panel_from_scene();
      return;
    }
    if (id == "selection.invert") {
      if (host_->invert_selection()) {
        host_->sync_inspectors_from_scene();
        host_->invalidate_map_overlays();
        host_->set_status_message("Selection inverted (next feature)");
      } else {
        host_->set_status_message("Selection invert: no features");
      }
      return;
    }
    if (id == "selection.zoom_to") {
      host_->browser_->run_tool_command("view.zoom_selection");
      return;
    }
    if (id == "selection.export_selected") {
      std::string path;
      if (host_->export_selection_geojson(&path)) {
        host_->set_status_message(std::string("Exported selection: ") + path);
      } else {
        host_->set_status_message("Selection export failed (nothing selected)");
      }
      return;
    }
    host_->set_status_message(std::string("Selection: ") + id);
  });
  host_->sync_selection_panel_from_scene();
}


void InspectComposer::wire_layer_properties_panel() {
  if (!host_->layer_properties_panel_ || !host_->layer_properties_panel_->symbology()) {
    return;
  }
  auto* symbology = host_->layer_properties_panel_->symbology();
  symbology->set_paint({{"line-color", "#3388ff"},
                        {"line-width", "1.5"},
                        {"fill-color", "#88aacc"},
                        {"fill-opacity", "0.6"}});
  symbology->set_apply_handler(
      [this](const ui::views::SymbologyPanel::PaintKv& paint) {
        auto* doc_scene = host_->browser_->document();
        std::string layer_name;
        if (host_->layer_properties_panel_ && host_->layer_properties_panel_->symbology()) {
          layer_name = host_->layer_properties_panel_->symbology()->layer_token();
        }
        if (layer_name.empty() && !doc_scene->layers().empty()) {
          layer_name = doc_scene->active_layer_id();
          for (const auto& layer : doc_scene->layers()) {
            if (layer.id == layer_name || layer.name == layer_name) {
              layer_name = layer.name;
              break;
            }
          }
          if (layer_name.empty()) {
            layer_name = doc_scene->layers().front().name;
          }
        }
        auto doc = std::make_shared<gis::style::StyleDocument>();
        if (const gis::style::StyleDocument* existing =
                doc_scene->style_document()) {
          *doc = *existing;
        } else {
          doc->version = 8;
          doc->name = "symbology-panel";
        }
        gis::style::StyleLayer* target = nullptr;
        for (auto& layer : doc->layers) {
          if (layer.source_layer == layer_name || layer.id == layer_name) {
            target = &layer;
            break;
          }
        }
        if (!target) {
          gis::style::StyleLayer created;
          created.id = layer_name.empty() ? "panel-layer" : layer_name;
          created.source_layer = layer_name;
          created.type = gis::style::LayerType::kLine;
          for (const auto& kv : paint) {
            if (kv.first.find("fill") != std::string::npos) {
              created.type = gis::style::LayerType::kFill;
              break;
            }
          }
          doc->layers.push_back(std::move(created));
          target = &doc->layers.back();
        }
        target->paint.clear();
        for (const auto& kv : paint) {
          target->paint[kv.first] = kv.second;
        }
        doc_scene->set_style_document(std::move(doc));
        host_->sync_legend_panel_from_scene();
        host_->invalidate_map_overlays();
        host_->set_status_message(std::string("Symbology applied: ") + layer_name);
      });
  host_->sync_layer_properties_from_scene();
}


void InspectComposer::wire_legend_panel() {
  if (!host_->legend_panel_) {
    return;
  }
  host_->legend_panel_->set_toggle([this](const std::string& id, bool visible) {
    if (host_->browser_->document()->set_layer_visible(id, visible)) {
      host_->sync_catalog_from_scene();
      host_->invalidate_map_overlays();
      host_->set_status_message(std::string("Layer ") + id +
                         (visible ? " visible" : " hidden"));
    } else {
      host_->set_status_message(std::string("Legend toggle failed: ") + id);
    }
  });
  host_->sync_legend_panel_from_scene();
}


bool InspectComposer::try_consume_measure_draft(const tool::Draft& draft) {
  if (!host_->measure_armed_ || !host_->measure_panel_ || !host_->browser_ ||
      !host_->browser_->view_frame()) {
    return false;
  }
  if (draft.points.size() < 2) {
    return false;
  }
  std::vector<std::pair<double, double>> map_pts;
  map_pts.reserve(draft.points.size());
  for (const auto& pt : draft.points) {
    double mx = 0;
    double my = 0;
    host_->browser_->view_frame()->view_to_map(pt.x_px, pt.y_px, &mx, &my);
    map_pts.emplace_back(mx, my);
  }

  const bool china = host_->browser_->document()->has_china_extent();
  auto segment_m = [china](double x0, double y0, double x1, double y1) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    if (!china) {
      return std::sqrt(dx * dx + dy * dy);
    }
    const double lat = 0.5 * (-y0 + -y1);
    const double m_per_deg_lat = 111320.0;
    const double m_per_deg_lon =
        111320.0 * std::cos(lat * 3.14159265358979323846 / 180.0);
    const double east = dx * m_per_deg_lon;
    const double north = (-dy) * m_per_deg_lat;
    return std::sqrt(east * east + north * north);
  };

  const auto mode = host_->measure_panel_->mode();
  std::vector<ui::views::MeasurePanel::ResultRow> rows;
  char buf[64];

  if (mode == ui::views::MeasurePanel::Mode::kLength ||
      mode == ui::views::MeasurePanel::Mode::kAzimuth) {
    double total = 0;
    for (size_t i = 1; i < map_pts.size(); ++i) {
      total += segment_m(map_pts[i - 1].first, map_pts[i - 1].second,
                         map_pts[i].first, map_pts[i].second);
    }
    std::snprintf(buf, sizeof(buf), "%.3f", total);
    rows.push_back({"length_m", buf});
    if (mode == ui::views::MeasurePanel::Mode::kAzimuth &&
        map_pts.size() >= 2) {
      const auto& a = map_pts.front();
      const auto& b = map_pts.back();
      const double dx = b.first - a.first;
      const double dy = -(b.second - a.second);
      double deg = std::atan2(dx, dy) * 180.0 / 3.14159265358979323846;
      if (deg < 0) {
        deg += 360.0;
      }
      std::snprintf(buf, sizeof(buf), "%.2f", deg);
      rows.push_back({"azimuth_deg", buf});
    }
    host_->measure_panel_->set_unit_text("m");
  } else {
    double area = 0;
    for (size_t i = 0; i < map_pts.size(); ++i) {
      const auto& p0 = map_pts[i];
      const auto& p1 = map_pts[(i + 1) % map_pts.size()];
      area += p0.first * (-p1.second) - p1.first * (-p0.second);
    }
    area = std::fabs(area) * 0.5;
    if (china) {
      const double lat = -map_pts.front().second;
      const double m_per_deg_lat = 111320.0;
      const double m_per_deg_lon =
          111320.0 * std::cos(lat * 3.14159265358979323846 / 180.0);
      area *= m_per_deg_lon * m_per_deg_lat;
    }
    std::snprintf(buf, sizeof(buf), "%.3f", area);
    rows.push_back({"area_m2", buf});
    host_->measure_panel_->set_unit_text("m虏");
  }

  host_->measure_panel_->set_results(std::move(rows));
  host_->show_inspector_tab_index(host_->measure_tab_);
  host_->set_status_message("Measure updated");
  return true;
}


void InspectComposer::sync_selection_panel_from_scene() {
  if (!host_->selection_panel_ || !host_->browser_) {
    return;
  }
  const MapScene::Feature* sel = host_->browser_->document()->selected_feature();
  host_->selection_panel_->set_count(sel ? 1 : 0);
  std::vector<ui::views::SelectionPanel::LayerSummary> layers;
  for (const auto& layer : host_->browser_->document()->layers()) {
    int n = 0;
    for (const auto& f : layer.features) {
      if (sel && f.id.len == sel->id.len &&
          std::memcmp(f.id.bytes, sel->id.bytes, sizeof(f.id.bytes)) == 0) {
        ++n;
      }
    }
    if (n > 0 || layer.visible) {
      layers.push_back({layer.id, layer.name, n});
    }
  }
  host_->selection_panel_->set_layers(std::move(layers));
}


void InspectComposer::sync_legend_panel_from_scene() {
  if (!host_->legend_panel_ || !host_->browser_) {
    return;
  }
  std::vector<ui::views::LegendPanel::Entry> entries;
  const double scale =
      host_->browser_->view_frame() ? host_->browser_->view_frame()->scale() : 8.0;
  for (const auto& layer : host_->browser_->document()->layers()) {
    ui::views::LegendPanel::Entry e;
    e.id = layer.id;
    e.label = layer.name;
    e.visible = layer.visible;
    e.swatch = "#888888";
    if (!layer.features.empty()) {
      COLORREF fill = RGB(136, 136, 136);
      COLORREF stroke = fill;
      int width = 1;
      if (host_->browser_->document()->style_colors_for_feature(
              layer, layer.features.front(), scale, &fill, &stroke, &width)) {
        char hex[16];
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", GetRValue(stroke),
                      GetGValue(stroke), GetBValue(stroke));
        e.swatch = hex;
      }
    }
    entries.push_back(std::move(e));
  }
  host_->legend_panel_->set_entries(std::move(entries));
}


void InspectComposer::sync_layer_properties_from_scene() {
  if (!host_->layer_properties_panel_ || !host_->browser_) {
    return;
  }
  std::string source = "(document)";
  std::string layer_token;
  std::string geom = "unknown";
  for (const auto& layer : host_->browser_->document()->layers()) {
    if (layer.id == host_->browser_->document()->active_layer_id() ||
        layer_token.empty()) {
      layer_token = layer.id.empty() ? layer.name : layer.id;
      if (!layer.features.empty()) {
        switch (layer.features.front().kind) {
          case MapScene::GeomKind::kPoint:
            geom = "point";
            break;
          case MapScene::GeomKind::kLine:
            geom = "line";
            break;
          case MapScene::GeomKind::kPolygon:
            geom = "fill";
            break;
          case MapScene::GeomKind::kText:
            geom = "symbol";
            break;
        }
      }
      if (layer.id == host_->browser_->document()->active_layer_id()) {
        break;
      }
    }
  }
  host_->layer_properties_panel_->set_source_text(source);
  if (auto* symbology = host_->layer_properties_panel_->symbology()) {
    symbology->set_layer(layer_token, geom);
  }
}


bool InspectComposer::invert_selection() {
  if (!host_->browser_) {
    return false;
  }
  const MapScene::Feature* sel = host_->browser_->document()->selected_feature();
  bool take_next = (sel == nullptr);
  const MapScene::Feature* first = nullptr;
  for (const auto& layer : host_->browser_->document()->layers()) {
    for (const auto& f : layer.features) {
      if (!first) {
        first = &f;
      }
      if (take_next) {
        return host_->browser_->document()->select_feature(f.id);
      }
      if (sel && f.id.len == sel->id.len &&
          std::memcmp(f.id.bytes, sel->id.bytes, sizeof(f.id.bytes)) == 0) {
        take_next = true;
      }
    }
  }
  if (first) {
    return host_->browser_->document()->select_feature(first->id);
  }
  return false;
}


bool InspectComposer::export_selection_geojson(std::string* out_path) {
  if (!out_path || !host_->browser_) {
    return false;
  }
  const MapScene::Feature* sel = host_->browser_->document()->selected_feature();
  if (!sel || sel->points.empty()) {
    return false;
  }
  namespace fs = std::filesystem;
  const fs::path path =
      fs::temp_directory_path() / "smartgis_selection_export.geojson";
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  const char* gtype = "LineString";
  if (sel->kind == MapScene::GeomKind::kPoint || sel->points.size() == 1) {
    gtype = "Point";
  } else if (sel->kind == MapScene::GeomKind::kPolygon) {
    gtype = "Polygon";
  }
  out << "{\"type\":\"FeatureCollection\",\"features\":[{"
         "\"type\":\"Feature\",\"properties\":{\"id\":\""
      << MapScene::feature_token(sel->id)
      << "\"},\"geometry\":{\"type\":\"" << gtype << "\",\"coordinates\":";
  if (std::strcmp(gtype, "Point") == 0) {
    out << "[" << sel->points.front().x << "," << -sel->points.front().y
        << "]";
  } else if (std::strcmp(gtype, "Polygon") == 0) {
    out << "[[";
    for (size_t i = 0; i < sel->points.size(); ++i) {
      if (i) {
        out << ",";
      }
      out << "[" << sel->points[i].x << "," << -sel->points[i].y << "]";
    }
    if (!sel->points.empty()) {
      out << ",[" << sel->points.front().x << "," << -sel->points.front().y
          << "]";
    }
    out << "]]";
  } else {
    out << "[";
    for (size_t i = 0; i < sel->points.size(); ++i) {
      if (i) {
        out << ",";
      }
      out << "[" << sel->points[i].x << "," << -sel->points[i].y << "]";
    }
    out << "]";
  }
  out << "}}]}";
  if (!out) {
    return false;
  }
  *out_path = path.string();
  return true;
}


}  // namespace app
