// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/present/present.h"

#include "content/browser/document/map_scene.h"
#include "content/public/gis_document.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace {

bool append_map_point(content::GisDocument* doc, double x, double y,
                      const char* color) {
  if (!doc || !color) {
    return false;
  }
  tool::Draft draft;
  draft.kind = tool::DraftKind::kPoint;
  draft.points.push_back({0, 0});
  const content::FeatureId id = doc->append_from_draft(
      draft, "draw.point",
      [x, y](int, int, double* map_x, double* map_y) {
        if (!map_x || !map_y) {
          return;
        }
        *map_x = x;
        *map_y = -y;
      });
  if (id.len == 0) {
    return false;
  }
  const std::string token = content::GisDocument::feature_token(id);
  doc->update_feature_field(token, "color", color);
  doc->update_feature_field(token, "name", "");
  return true;
}

}  // namespace

bool present_geochem(content::GisDocument* doc,
                     const GeochemCommit& commit,
                     std::string* err) {
  if (!doc) {
    if (err) {
      *err = "no_doc";
    }
    return false;
  }
  if (!commit.samples.ok || commit.samples.samples.empty()) {
    if (err) {
      *err = "no_samples";
    }
    return false;
  }

  doc->remove_layer("geochem_heat_raster");
  doc->remove_layer("geochem_samples");
  if (!apply_style_resource(doc, "smartgis.geochem", "geochem.style.json")) {
    if (err) {
      *err = "style_failed";
    }
    return false;
  }

  const int elem_idx =
      gis::detail::geochem_element_index(commit.samples, commit.element);

  if (commit.has_idw && commit.idw.ok && !commit.idw.values.empty()) {
    if (!doc->create_layer("geochem_heat_raster", "Polygon")) {
      if (err) {
        *err = "heat_layer_failed";
      }
      return false;
    }
    const int w = commit.idw.width;
    const int h = commit.idw.height;
    const double* gt = commit.idw.geotransform;
    double vmin = 0;
    double vmax = 0;
    bool range_set = false;
    for (float raw : commit.idw.values) {
      if (!std::isfinite(raw) || raw <= -9998.5f) {
        continue;
      }
      if (!range_set) {
        vmin = vmax = static_cast<double>(raw);
        range_set = true;
      } else {
        vmin = std::min(vmin, static_cast<double>(raw));
        vmax = std::max(vmax, static_cast<double>(raw));
      }
    }
    constexpr int kMaxCells = 48 * 48;
    const int step =
        (w * h > kMaxCells)
            ? std::max(1, static_cast<int>(std::ceil(
                              std::sqrt(static_cast<double>(w * h) /
                                        static_cast<double>(kMaxCells)))))
            : 1;
    for (int row = 0; row < h; row += step) {
      for (int col = 0; col < w; col += step) {
        const size_t i = static_cast<size_t>(row * w + col);
        if (i >= commit.idw.values.size()) {
          continue;
        }
        const float raw = commit.idw.values[i];
        if (!std::isfinite(raw) || raw <= -9998.5f) {
          continue;
        }
        const double x0 = gt[0] + gt[1] * col;
        const double y0 = gt[3] + gt[5] * row;
        const double x1 = gt[0] + gt[1] * (col + step);
        const double y1 = gt[3] + gt[5] * (row + step);
        const double min_x = std::min(x0, x1);
        const double max_x = std::max(x0, x1);
        const double min_y = std::min(y0, y1);
        const double max_y = std::max(y0, y1);
        const std::vector<std::pair<double, double>> ring = {
            {min_x, min_y},
            {max_x, min_y},
            {max_x, max_y},
            {min_x, max_y},
            {min_x, min_y},
        };
        const double score = gis::detail::geochem_heat_score(
            static_cast<double>(raw), vmin, vmax);
        char heat_buf[32];
        const char* heat =
            gis::detail::format_geochem_heat(score, heat_buf, sizeof(heat_buf));
        if (!heat || !heat[0]) {
          continue;
        }
        if (!append_map_polygon(doc, ring, heat)) {
          continue;
        }
      }
    }
  }

  const size_t n = commit.samples.samples.size();
  if (!doc->create_layer("geochem_samples", "Point")) {
    if (err) {
      *err = "samples_layer_failed";
    }
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    const auto& s = commit.samples.samples[i];
    uint32_t packed = 0xff3498db;
    if (elem_idx >= 0 && static_cast<size_t>(elem_idx) < s.values.size() &&
        commit.legend.ok) {
      const int ci = gis::detail::geochem_grade_class_index(
          commit.legend, s.values[static_cast<size_t>(elem_idx)]);
      if (ci >= 0) {
        packed = commit.legend.classes[static_cast<size_t>(ci)].rgba;
      }
    }
    char color[8] = {};
    std::snprintf(color, sizeof(color), "#%02x%02x%02x",
                  static_cast<unsigned>((packed >> 16) & 0xff),
                  static_cast<unsigned>((packed >> 8) & 0xff),
                  static_cast<unsigned>(packed & 0xff));
    if (!append_map_point(doc, s.x, s.y, color)) {
      if (err) {
        *err = "samples_layer_failed";
      }
      return false;
    }
  }
  return true;
}

bool read_geochem_active_layer(content::MapScene* doc,
                               const std::string& element,
                               gis::detail::GeochemSampleSet* out,
                               std::string* err) {
  if (!doc || !out) {
    if (err) {
      *err = "no_doc";
    }
    return false;
  }
  out->ok = false;
  out->samples.clear();
  out->element_names.clear();
  out->element_names.push_back(element.empty() ? "value" : element);
  const std::string& active = doc->active_layer_id();
  for (const content::detail::MapLayer& layer : doc->layers()) {
    if (!active.empty() && layer.id != active && layer.name != active) {
      continue;
    }
    for (const content::detail::MapFeature& f : layer.features) {
      if (f.kind != content::detail::GeomKind::kPoint || f.points.empty()) {
        continue;
      }
      gis::detail::GeochemSample sample;
      sample.x = f.points.front().x;
      sample.y = -f.points.front().y;
      if (const char* id = content::detail::named_field_value(f, "id")) {
        sample.id = id;
      } else if (const char* name =
                     content::detail::named_field_value(f, "name")) {
        sample.id = name;
      }
      const char* raw = content::detail::named_field_value(f, element.c_str());
      if (!raw) {
        raw = content::detail::named_field_value(f, "z");
      }
      if (!raw) {
        raw = content::detail::named_field_value(f, "value");
      }
      if (!raw) {
        continue;
      }
      char* end = nullptr;
      const double v = std::strtod(raw, &end);
      if (end == raw || !std::isfinite(v)) {
        continue;
      }
      sample.values.push_back(v);
      out->samples.push_back(std::move(sample));
    }
    if (!out->samples.empty()) {
      break;
    }
  }
  if (out->samples.empty()) {
    if (err) {
      *err = "no_point_features";
    }
    out->error = "no_point_features";
    return false;
  }
  out->ok = true;
  return true;
}

}  // namespace plugin
