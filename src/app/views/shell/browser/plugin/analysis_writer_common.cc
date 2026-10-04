// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_common.h"

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "content/browser/document/map_scene.h"
#include "gis/style/document/style_document.h"
#include "tool/draft/draft.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {

bool refresh_ui_after_layer(BrowserUiDelegate* ui) {
  (void)ui;
  return true;
}

// Tiny triangle stand-in so sphere/water/terrain cmds paint on map2d.
bool add_standin_mesh(content::MapScene* doc,
                      BrowserUiDelegate* ui,
                      const char* name,
                      double lon,
                      double lat,
                      double half_deg) {
  if (!doc || !name || half_deg <= 0.0) {
    return false;
  }
  const double xyz[9] = {
      lon - half_deg, lat - half_deg, 0.0, lon + half_deg, lat - half_deg, 0.0,
      lon,            lat + half_deg, 0.0,
  };
  const int tris[3] = {0, 1, 2};
  if (!doc->add_triangle_layer(name, xyz, 3, tris, 1)) {
    return false;
  }
  return refresh_ui_after_layer(ui);
}

bool append_map_polygon(content::MapScene* doc,
                        const std::vector<std::pair<double, double>>& ring,
                        const char* heat) {
  if (!doc || ring.size() < 3 || !heat) {
    return false;
  }
  tool::Draft draft;
  draft.kind = tool::DraftKind::kPolygon;
  draft.points.reserve(ring.size());
  for (size_t i = 0; i < ring.size(); ++i) {
    draft.points.push_back({static_cast<int32_t>(i), 0});
  }
  const content::FeatureId id = doc->append_from_draft(
      draft, "draw.polygon",
      [&ring](int view_x, int, double* map_x, double* map_y) {
        const size_t i = static_cast<size_t>(view_x);
        if (i >= ring.size() || !map_x || !map_y) {
          return;
        }
        *map_x = ring[i].first;
        *map_y = -ring[i].second;
      });
  if (id.len == 0) {
    return false;
  }
  const std::string token = content::MapScene::feature_token(id);
  doc->update_feature_field(token, "heat", heat);
  // Suppress auto layer-id labels (paint_labels falls back to polygons).
  doc->update_feature_field(token, "name", "");
  return true;
}

bool apply_style_json(content::MapScene* doc, const char* json) {
  if (!doc || !json) {
    return false;
  }
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(json, style.get())) {
    doc->clear_style_document();
    return false;
  }
  doc->set_style_document(std::move(style));
  return true;
}

bool append_map_polyline(content::MapScene* doc,
                         const std::vector<std::pair<double, double>>& xy,
                         const char* frame_tag) {
  return append_map_polyline(doc, xy, frame_tag, "path", nullptr);
}

bool append_map_polyline(content::MapScene* doc,
                         const std::vector<std::pair<double, double>>& xy,
                         const char* frame_tag,
                         const char* type_field,
                         const char* heat) {
  if (!doc || xy.size() < 2) {
    return false;
  }
  tool::Draft draft;
  draft.kind = tool::DraftKind::kLineString;
  draft.points.reserve(xy.size());
  for (size_t i = 0; i < xy.size(); ++i) {
    draft.points.push_back({static_cast<int32_t>(i), 0});
  }
  const content::FeatureId id = doc->append_from_draft(
      draft, "draw.linestring",
      [&xy](int view_x, int, double* map_x, double* map_y) {
        const size_t i = static_cast<size_t>(view_x);
        if (i >= xy.size() || !map_x || !map_y) {
          return;
        }
        *map_x = xy[i].first;
        *map_y = -xy[i].second;
      });
  if (id.len == 0) {
    return false;
  }
  const std::string token = content::MapScene::feature_token(id);
  doc->update_feature_field(token, "type",
                            (type_field && type_field[0]) ? type_field : "path");
  if (frame_tag) {
    doc->update_feature_field(token, "frame", frame_tag);
  }
  if (heat && heat[0]) {
    doc->update_feature_field(token, "heat", heat);
  }
  // Suppress auto label fallback on network/path polylines.
  doc->update_feature_field(token, "name", "");
  return true;
}

}  // namespace detail
}  // namespace app
