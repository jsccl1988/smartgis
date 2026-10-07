// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Present style selection on a StyleDocument. No GisScene and no bake.

#ifndef GIS_STYLE_CARTO_RESOLVE_H_
#define GIS_STYLE_CARTO_RESOLVE_H_

#include "gis/gis_export.h"

namespace gis {
namespace style {

struct StyleDocument;
struct StyleLayer;

GIS_EXPORT bool style_is_china_city_pack(const StyleDocument* doc);
GIS_EXPORT bool style_has_carto_slots(const StyleDocument* doc);

// Parsed default_carto_style_json(), process-lifetime, call_once.
GIS_EXPORT const StyleDocument* default_carto_style();

// China city packs and a null document fall back to default_carto_style().
// |use_carto| is true when batches should use carto source-layer slots.
GIS_EXPORT const StyleDocument* resolve_present_style(
    const StyleDocument* scene_style, bool* use_carto);

GIS_EXPORT const StyleLayer* find_hillshade_layer(const StyleDocument* style,
                                                   double zoom);

}  // namespace style
}  // namespace gis

#endif  // GIS_STYLE_CARTO_RESOLVE_H_
