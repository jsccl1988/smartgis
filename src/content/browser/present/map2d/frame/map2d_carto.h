// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_CARTO_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_CARTO_H_

#include <cstdint>

namespace gis {
namespace style {
struct StyleDocument;
struct StyleLayer;
}  // namespace style
}  // namespace gis

namespace content {
namespace detail {

// Host texture id for the DEM hillshade raster DrawItem ('HSHD').
inline constexpr uint32_t kMap2dHillshadeTextureKey = 0x48534844u;

bool style_is_china_city_pack(const gis::style::StyleDocument* doc);
bool style_has_carto_slots(const gis::style::StyleDocument* doc);

const gis::style::StyleDocument* default_carto_style();

// Picks the style Layout::build should see. China city packs and missing
// documents fall back to the default carto JSON. |use_carto| is true when
// batches should use carto source-layer slots.
const gis::style::StyleDocument* resolve_present_style(
    const gis::style::StyleDocument* scene_style, bool* use_carto);

const gis::style::StyleLayer* find_hillshade_layer(
    const gis::style::StyleDocument* style, double zoom);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_FRAME_MAP2D_CARTO_H_
