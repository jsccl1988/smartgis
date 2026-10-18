// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Present style selection. The painted default is the file-backed vista
// document (out/ui/carto/default.style.json). gis::style keeps a separate
// embedded copy that had drifted to a single yellow road ramp.

#include "content/browser/present/map2d/frame/carto/map2d_carto.h"

#include "gis/style/carto/resolve.h"
#include "vista/component/map/carto/resolve.h"

namespace content {
namespace detail {

bool style_uses_area_point_line_slots(const gis::style::StyleDocument* doc) {
  return gis::style::style_uses_area_point_line_slots(doc);
}

bool style_has_carto_slots(const gis::style::StyleDocument* doc) {
  return gis::style::style_has_carto_slots(doc);
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
  return gis::style::find_hillshade_layer(style, zoom);
}

}  // namespace detail
}  // namespace content
