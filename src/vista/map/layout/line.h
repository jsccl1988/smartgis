// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for stroked line ribbons (serial or parallel tessellation).

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_LINE_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_LINE_H_

#include <vector>

#include "vista/map/frame.h"
#include "vista/map/layout/slice_key.h"

namespace vista {
namespace detail {

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapFrame* frame,
                const LayoutTile* clip_tile = nullptr);
void emit_line(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapFrame* frame, const LayoutTile* clip_tile = nullptr);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_LINE_H_
