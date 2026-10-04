// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_geochem.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "content/browser/document/map_scene.h"
#include "gis/carto/style/style_document.h"
#include "plugin/product/geochem/commands.h"
#include "tool/draft/draft.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

// Graded sample circles + full-extent IDW heat raster underlay.
constexpr const char* kGeochemStyleJson = R"json({
  "version": 8,
  "name": "geochem",
  "layers": [
    {"id":"bg","type":"background",
     "paint":{"background-color":"#f5f0e6","background-opacity":1}},
    {"id":"heat","type":"fill","source-layer":"geochem_heat_raster",
     "paint":{
       "fill-color":["interpolate",["linear"],["get","heat"],
         0,"#313695",25,"#74add1",50,"#fee090",75,"#f46d43",100,"#a50026"],
       "fill-opacity":0.72}},
    {"id":"heat-edge","type":"line","source-layer":"geochem_heat_raster",
     "paint":{"line-color":"#1a1a2e","line-width":0.4,"line-opacity":0.35}},
    {"id":"samples","type":"circle","source-layer":"geochem_samples",
     "paint":{"circle-color":["coalesce",["get","color"],"#3498db"],
       "circle-radius":7,"circle-opacity":1.0}}
  ]
})json";

}  // namespace

// Graded sample points + full-extent IDW heat raster (padded sample bbox).
bool commit_geochem(content::MapScene* doc,
                    BrowserUiDelegate* ui,
                    const plugin::GeochemCommit& commit,
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

  doc->clear();
  if (!apply_style_json(doc, kGeochemStyleJson)) {
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
    // Cap painted cells for map2d harness stability (same budget as orthogrid).
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
        const double score =
            gis::detail::geochem_heat_score(static_cast<double>(raw), vmin,
                                            vmax);
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
  std::vector<float> xyz(n * 3);
  std::vector<uint8_t> rgba(n * 4);
  for (size_t i = 0; i < n; ++i) {
    const auto& s = commit.samples.samples[i];
    xyz[i * 3] = static_cast<float>(s.x);
    xyz[i * 3 + 1] = static_cast<float>(-s.y);
    xyz[i * 3 + 2] = 0.f;
    uint32_t packed = 0xff3498db;
    if (elem_idx >= 0 &&
        static_cast<size_t>(elem_idx) < s.values.size() &&
        commit.legend.ok) {
      const int ci = gis::detail::geochem_grade_class_index(
          commit.legend, s.values[static_cast<size_t>(elem_idx)]);
      if (ci >= 0) {
        packed = commit.legend.classes[static_cast<size_t>(ci)].rgba;
      }
    }
    rgba[i * 4] = static_cast<uint8_t>((packed >> 16) & 0xff);
    rgba[i * 4 + 1] = static_cast<uint8_t>((packed >> 8) & 0xff);
    rgba[i * 4 + 2] = static_cast<uint8_t>(packed & 0xff);
    rgba[i * 4 + 3] = 255;
  }
  if (!doc->add_point_cloud_layer("geochem_samples", xyz.data(),
                                   static_cast<int>(n), rgba.data())) {
    if (err) {
      *err = "samples_layer_failed";
    }
    return false;
  }
  return refresh_ui_after_layer(ui);
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
      sample.y = -f.points.front().y;  // map Y is flipped
      if (const char* id = content::detail::named_field_value(f, "id")) {
        sample.id = id;
      } else if (const char* name =
                     content::detail::named_field_value(f, "name")) {
        sample.id = name;
      }
      double v = 0;
      const char* raw =
          content::detail::named_field_value(f, element.c_str());
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
      v = std::strtod(raw, &end);
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

void wire_geochem_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_geochem_writer(
      [browser](const plugin::GeochemCommit& commit, std::string* err) {
        return commit_geochem(&browser->session().document(), browser->ui(), commit, err);
      });
  plugin::set_geochem_layer_reader(
      [browser](const std::string& element, gis::detail::GeochemSampleSet* out,
             std::string* err) {
        return read_geochem_active_layer(&browser->session().document(), element, out,
                                         err);
      });

}

}  // namespace detail
}  // namespace app
