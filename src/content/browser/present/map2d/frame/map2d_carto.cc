// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Thin present forwarder over gis::style carto resolve. StyleDocument
// selection does not read GisScene.

#include "content/browser/present/map2d/frame/map2d_carto.h"

#include "gis/style/carto/resolve.h"

namespace content {
namespace detail {

bool style_is_china_city_pack(const gis::style::StyleDocument* doc) {
  return gis::style::style_is_china_city_pack(doc);
}

bool style_has_carto_slots(const gis::style::StyleDocument* doc) {
  return gis::style::style_has_carto_slots(doc);
}

const gis::style::StyleDocument* default_carto_style() {
  return gis::style::default_carto_style();
}

const gis::style::StyleDocument* resolve_present_style(
    const gis::style::StyleDocument* scene_style, bool* use_carto) {
  return gis::style::resolve_present_style(scene_style, use_carto);
}

const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom) {
  return gis::style::find_hillshade_layer(style, zoom);
}

}  // namespace detail
}  // namespace content
