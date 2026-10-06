// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/present/gis_present.h"

#include "content/public/gis_document.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
#include "tool/draft/draft.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace plugin {

bool append_map_polygon(content::GisDocument* doc,
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
  const std::string token = content::GisDocument::feature_token(id);
  doc->update_feature_field(token, "heat", heat);
  doc->update_feature_field(token, "name", "");
  return true;
}

bool apply_style_json(content::GisDocument* doc, const char* json) {
  if (!doc || !json) {
    return false;
  }
  return doc->apply_style_json(json);
}

bool apply_style_file(content::GisDocument* doc, const char* path) {
  if (!doc || !path || !path[0]) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  const std::string json((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  if (json.empty()) {
    return false;
  }
  return apply_style_json(doc, json.c_str());
}

bool apply_style_resource(content::GisDocument* doc,
                          std::string_view plugin_id,
                          std::string_view relative) {
  const std::string path = resolve_resource(plugin_id, relative);
  if (path.empty()) {
    return false;
  }
  return apply_style_file(doc, path.c_str());
}

bool append_map_polyline(content::GisDocument* doc,
                         const std::vector<std::pair<double, double>>& xy,
                         const char* frame_tag) {
  return append_map_polyline(doc, xy, frame_tag, "path", nullptr);
}

bool append_map_polyline(content::GisDocument* doc,
                         const std::vector<std::pair<double, double>>& xy,
                         const char* frame_tag, const char* type_field,
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
  const std::string token = content::GisDocument::feature_token(id);
  doc->update_feature_field(token, "type",
                            (type_field && type_field[0]) ? type_field : "path");
  if (frame_tag) {
    doc->update_feature_field(token, "frame", frame_tag);
  }
  if (heat && heat[0]) {
    doc->update_feature_field(token, "heat", heat);
  }
  doc->update_feature_field(token, "name", "");
  return true;
}

bool add_standin_mesh(content::GisDocument* doc, const char* name, double lon,
                      double lat, double half_deg) {
  if (!doc || !name || half_deg <= 0.0) {
    return false;
  }
  const double xyz[9] = {
      lon - half_deg, lat - half_deg, 0.0, lon + half_deg, lat - half_deg, 0.0,
      lon,            lat + half_deg, 0.0,
  };
  const int tris[3] = {0, 1, 2};
  return doc->add_triangle_mesh(name, xyz, 3, tris, 1);
}

}  // namespace plugin
