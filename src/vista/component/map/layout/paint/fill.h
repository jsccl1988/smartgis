// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for fill polygons and fill-extrusion prisms.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_FILL_H_
#define VISTA_COMPONENT_MAP_LAYOUT_FILL_H_

#include <vector>

#include "vista/component/map/ir.h"
#include "vista/component/map/layout/slice_key.h"

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

#endif  // VISTA_COMPONENT_MAP_LAYOUT_FILL_H_
