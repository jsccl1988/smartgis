// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for background, raster tiles, and hillshade underlays.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_RASTER_H_
#define VISTA_COMPONENT_MAP_LAYOUT_RASTER_H_

#include "vista/component/map/ir.h"

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

#endif  // VISTA_COMPONENT_MAP_LAYOUT_RASTER_H_
