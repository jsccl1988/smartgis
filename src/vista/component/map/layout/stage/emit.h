// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Painter dispatch for visible style layers. No RHI.

#ifndef VISTA_COMPONENT_MAP_LAYOUT_EMIT_H_
#define VISTA_COMPONENT_MAP_LAYOUT_EMIT_H_

#include <vector>

#include "vista/component/map/batch.h"
#include "vista/component/map/carto/collision.h"
#include "vista/component/map/draw.h"
#include "vista/component/map/layout.h"

namespace gis {
namespace style {
struct StyleLayer;
}
}  // namespace gis

namespace vista {
namespace detail {

void emit_visible_layers(const LayoutInput& in,
                         const std::vector<LayerBatch>& layers,
                         const std::vector<const gis::style::StyleLayer*>& visible,
                         LabelGrid* grid, MapIR* frame);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_EMIT_H_
