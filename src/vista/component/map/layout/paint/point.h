// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policies for circle marks and heatmap splat stand-ins.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_POINT_H_
#define VISTA_COMPONENT_MAP_LAYOUT_POINT_H_

#include <vector>

#include "vista/component/map/ir.h"
#include "vista/component/map/layout/slice_key.h"

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

#endif  // VISTA_COMPONENT_MAP_LAYOUT_POINT_H_
