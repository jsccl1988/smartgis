// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Present style selection on a StyleDocument. No GisScene and no bake.

#ifndef VISTA_COMPONENT_MAP_CARTO_RESOLVE_H_
#define VISTA_COMPONENT_MAP_CARTO_RESOLVE_H_

#include "vista/vista_export.h"

namespace gis {
namespace style {
struct StyleDocument;
struct StyleLayer;
}  // namespace style
}  // namespace gis

namespace vista {

VISTA_EXPORT bool style_is_china_city_pack(const gis::style::StyleDocument* doc);
VISTA_EXPORT bool style_has_carto_slots(const gis::style::StyleDocument* doc);

// Parsed default_carto_style_json(), process-lifetime, call_once.
VISTA_EXPORT const gis::style::StyleDocument* default_carto_style();

// China city packs and a null document fall back to default_carto_style().
// |use_carto| is true when batches should use carto source-layer slots.
VISTA_EXPORT const gis::style::StyleDocument* resolve_present_style(
    const gis::style::StyleDocument* scene_style, bool* use_carto);

VISTA_EXPORT const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom);

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_CARTO_RESOLVE_H_
