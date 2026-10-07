// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scheduler over vista carto resolve. StyleDocument selection does not
// read MapScene.

#include "content/browser/present/map2d/frame/map2d_carto.h"

#include "vista/component/map/carto/resolve.h"

namespace content {
namespace detail {

bool style_is_china_city_pack(const gis::style::StyleDocument* doc) {
  return vista::style_is_china_city_pack(doc);
}

bool style_has_carto_slots(const gis::style::StyleDocument* doc) {
  return vista::style_has_carto_slots(doc);
}

const gis::style::StyleDocument* default_carto_style() {
  return vista::default_carto_style();
}

const gis::style::StyleDocument* resolve_present_style(
    const gis::style::StyleDocument* scene_style, bool* use_carto) {
  return vista::resolve_present_style(scene_style, use_carto);
}

const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom) {
  return vista::find_hillshade_layer(style, zoom);
}

}  // namespace detail
}  // namespace content
