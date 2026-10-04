// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for circle marks and heatmap splat stand-ins.

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_POINT_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_POINT_H_

#include <vector>

#include "vista/map/ir.h"
#include "vista/map/layout/slice_key.h"

namespace vista {
namespace detail {

void emit_circles(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapIR* frame, const LayoutTile* clip_tile = nullptr);
void emit_heatmap(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapIR* frame, const LayoutTile* clip_tile = nullptr);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_POINT_H_
