// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for stroked line ribbons (serial or parallel tessellation).

#ifndef VISTA_COMPONENT_MAP_LAYOUT_LINE_H_
#define VISTA_COMPONENT_MAP_LAYOUT_LINE_H_

#include <vector>

#include "vista/component/map/ir.h"
#include "vista/component/map/layout/slice_key.h"

namespace vista {
namespace detail {

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapIR* frame,
                const LayoutTile* clip_tile = nullptr,
                bool intersect_clip = true);
void emit_line(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapIR* frame, const LayoutTile* clip_tile = nullptr,
               bool intersect_clip = true);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_LINE_H_
