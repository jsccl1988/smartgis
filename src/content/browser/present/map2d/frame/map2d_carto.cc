// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_carto.h"

#include <mutex>

#include "gis/style/document/style_document.h"
#include "gis/style/eval/style_rules.h"
#include "vista/map/ir.h"

namespace content {
namespace detail {

bool style_is_china_city_pack(const gis::style::StyleDocument* doc) {
  if (!doc || doc->layers.empty()) {
    return false;
  }
  bool has_area_or_point = false;
  bool has_land_or_river = false;
  for (const gis::style::StyleLayer& layer : doc->layers) {
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

bool style_has_carto_slots(const gis::style::StyleDocument* doc) {
  if (!doc) {
    return false;
  }
  for (const gis::style::StyleLayer& layer : doc->layers) {
    if (layer.source_layer == "land" || layer.source_layer == "river" ||
        layer.source_layer == "road" || layer.source_layer == "label") {
      return true;
    }
  }
  return false;
}

const gis::style::StyleDocument* default_carto_style() {
  static std::once_flag carto_once;
  static gis::style::StyleDocument carto_doc;
  std::call_once(carto_once, [] {
    (void)gis::style::parse_style_document(vista::default_carto_style_json(),
                                           &carto_doc);
  });
  return &carto_doc;
}

const gis::style::StyleDocument* resolve_present_style(
    const gis::style::StyleDocument* scene_style, bool* use_carto) {
  const bool china_city_pack = style_is_china_city_pack(scene_style);
  const bool carto_slots = style_has_carto_slots(scene_style);
  const bool carto = scene_style == nullptr || china_city_pack || carto_slots;
  if (use_carto) {
    *use_carto = carto;
  }
  const gis::style::StyleDocument* style = scene_style;
  if (china_city_pack) {
    style = nullptr;
  }
  if (!style) {
    style = default_carto_style();
  }
  return style;
}

const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom) {
  if (!style) {
    return nullptr;
  }
  for (const gis::style::StyleLayer& layer : style->layers) {
    if (layer.type != gis::style::LayerType::kHillshade) {
      continue;
    }
    if (!gis::style::layer_matches_zoom(layer, zoom)) {
      continue;
    }
    return &layer;
  }
  return nullptr;
}

}  // namespace detail
}  // namespace content
