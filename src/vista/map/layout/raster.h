// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for background, raster tiles, and hillshade underlays.

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_RASTER_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_RASTER_H_

#include "vista/map/ir.h"

namespace vista {
namespace detail {

void apply_background(const gis::style::StyleLayer& layer, double zoom,
                      MapIR* frame);
void emit_raster(const gis::style::StyleLayer& layer, const LayoutInput& in,
                 MapIR* frame);
void emit_hillshade(const gis::style::StyleLayer& layer, const LayoutInput& in,
                    MapIR* frame);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_RASTER_H_
