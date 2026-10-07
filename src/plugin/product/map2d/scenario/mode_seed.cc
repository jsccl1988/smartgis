// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/mode_seed.h"

#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/product/map2d/scenario/sample.h"
#include "plugin/product/map2d/seed/seed.h"
#include "plugin/runtime/host/capability/shell.h"
#include "content/browser/document/gis_scene.h"
#include "gis/style/document/style_document.h"
#include "gis/style/style_types.h"

#include <cstdio>
#include <memory>
#include <string>

namespace plugin {
namespace detail {
namespace {

void ensure_china_maplibre_carto(HarnessShell& browser) {
  content::GisScene* doc = browser.document();
  if (!doc || !doc->has_china_extent()) {
    return;
  }
  const std::shared_ptr<gis::style::StyleDocument> style =
      doc->style_document_shared();
  if (!style) {
    return;
  }
  bool has_area_or_point = false;
  bool has_land_or_river = false;
  bool has_product_slot = false;
  const std::size_t n = style->layers.size();
  if (n > 4096) {
    return;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const gis::style::StyleLayer& layer = style->layers[i];
    if (layer.source_layer == "land" || layer.source_layer == "river" ||
        layer.source_layer == "label") {
      has_land_or_river = true;
    }
    if (layer.source_layer == "area" || layer.source_layer == "point" ||
        layer.source_layer == "line") {
      has_area_or_point = true;
    }
    if (layer.source_layer.find("geochem") != std::string::npos ||
        layer.source_layer.find("flood") != std::string::npos ||
        layer.source_layer.find("traffic") != std::string::npos ||
        layer.source_layer.find("orthogrid") != std::string::npos) {
      has_product_slot = true;
    }
  }
  if (has_product_slot) {
    return;
  }
  if (has_area_or_point && !has_land_or_river) {
    doc->clear_style_document();
  }
}

bool execute_map2d_seed(HarnessShell& browser, const char* mode_name) {
  content::PluginHost* host = browser.plugin_host();
  if (!host || !mode_name || !mode_name[0]) {
    return false;
  }
  const std::string payload = std::string("{\"mode\":\"") + mode_name + "\"}";
  return seed_map2d_from_json(host, payload);
}

}  // namespace

int seed_map2d_mode(HarnessShell& browser, ScenarioMode mode) {
  if (mode != ScenarioMode::kChina && mode != ScenarioMode::kAlign &&
      mode != ScenarioMode::kOrthogrid) {
    std::fprintf(stderr, "map2d-showcase: unsupported mode\n");
    return 53;
  }

  if (mode == ScenarioMode::kOrthogrid) {
    if (!execute_map2d_seed(browser, "orthogrid")) {
      std::fprintf(stderr, "map2d-showcase: map2d.seed orthogrid failed\n");
      return 55;
    }
    map2d_mark("orthogrid-ok");
    map2d_mark("map2d-seed-plugin");
    return 0;
  }

  if (mode == ScenarioMode::kAlign) {
    if (!try_open_china_sample(browser)) {
      if (browser.document()) {
        browser.document()->seed_default();
      }
    }
    if (!browser.document() || browser.document()->feature_count() < 3) {
      std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
      return 55;
    }
    map2d_mark("china-ok");
    if (execute_map2d_seed(browser, "align")) {
      map2d_mark("map2d-seed-plugin");
      return 0;
    }
    if (!try_load_align_style(browser)) {
      std::fprintf(stderr, "map2d-showcase: style_align.json load failed\n");
      return 55;
    }
    return 0;
  }

  if (!try_open_china_sample(browser)) {
    if (browser.document()) {
      browser.document()->seed_default();
    }
  }
  if (!browser.document() || browser.document()->feature_count() < 3) {
    std::fprintf(stderr, "map2d-showcase: china sample open failed\n");
    return 55;
  }
  map2d_mark("china-ok");
  if (execute_map2d_seed(browser, "china")) {
    map2d_mark("map2d-seed-plugin");
  }
  ensure_china_maplibre_carto(browser);
  map2d_mark("style-carto-default");
  return 0;
}

}  // namespace detail
}  // namespace plugin
