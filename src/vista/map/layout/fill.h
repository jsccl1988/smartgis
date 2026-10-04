// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for fill polygons and fill-extrusion prisms.

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_FILL_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_FILL_H_

#include <vector>

#include "vista/map/ir.h"
#include "vista/map/layout/slice_key.h"

namespace vista {
namespace detail {

void emit_fill_extrusion(const gis::style::StyleLayer& layer,
                         const LayoutInput& in,
                         const std::vector<LayerBatch>& layers, double wupp,
                         MapIR* frame, const LayoutTile* clip_tile = nullptr);
void emit_fills(const std::vector<const gis::style::StyleLayer*>& fill_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame,
                const LayoutTile* clip_tile = nullptr);
void emit_fill(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapIR* frame, const LayoutTile* clip_tile = nullptr);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_FILL_H_
