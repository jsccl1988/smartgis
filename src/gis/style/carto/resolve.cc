// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/style/carto/resolve.h"

#include <mutex>

#include "gis/style/carto/default_style.h"
#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"

namespace gis {
namespace style {

bool style_is_china_city_pack(const StyleDocument* doc) {
  if (!doc || doc->layers.empty()) {
    return false;
  }
  bool has_area_or_point = false;
  bool has_land_or_river = false;
  for (const StyleLayer& layer : doc->layers) {
    if (layer.source_layer == "land" || layer.source_layer == "river" ||
        layer.source_layer == "label") {
      has_land_or_river = true;
    }
    if (layer.source_layer == "area" || layer.source_layer == "point" ||
        layer.source_layer == "line") {
      has_area_or_point = true;
    }
  }
  return has_area_or_point && !has_land_or_river;
}

bool style_has_carto_slots(const StyleDocument* doc) {
  if (!doc) {
    return false;
  }
  for (const StyleLayer& layer : doc->layers) {
    if (layer.source_layer == "land" || layer.source_layer == "river" ||
        layer.source_layer == "road" || layer.source_layer == "label") {
      return true;
    }
  }
  return false;
}

const StyleDocument* default_carto_style() {
  static std::once_flag carto_once;
  static StyleDocument carto_doc;
  std::call_once(carto_once, [] {
    (void)parse_style_document(default_carto_style_json(), &carto_doc);
  });
  return &carto_doc;
}

const StyleDocument* resolve_present_style(const StyleDocument* scene_style,
                                           bool* use_carto) {
  const bool china_city_pack = style_is_china_city_pack(scene_style);
  const bool carto_slots = style_has_carto_slots(scene_style);
  const bool carto = scene_style == nullptr || china_city_pack || carto_slots;
  if (use_carto) {
    *use_carto = carto;
  }
  const StyleDocument* style = scene_style;
  if (china_city_pack) {
    style = nullptr;
  }
  if (!style) {
    style = default_carto_style();
  }
  return style;
}

const StyleLayer* find_hillshade_layer(const StyleDocument* style,
                                       double zoom) {
  if (!style) {
    return nullptr;
  }
  for (const StyleLayer& layer : style->layers) {
    if (layer.type != LayerType::kHillshade) {
      continue;
    }
    if (!layer_matches_zoom(layer, zoom)) {
      continue;
    }
    return &layer;
  }
  return nullptr;
}

}  // namespace style
}  // namespace gis
