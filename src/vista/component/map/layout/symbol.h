// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policy for symbol icons and text labels (collision + emit).

#ifndef VISTA_COMPONENT_MAP_LAYOUT_SYMBOL_H_
#define VISTA_COMPONENT_MAP_LAYOUT_SYMBOL_H_

#include <vector>

#include "vista/component/map/detail/collision.h"
#include "vista/component/map/ir.h"

namespace vista {
namespace detail {

void emit_symbols(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, float fblc,
                  LabelGrid* grid, MapIR* frame);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_SYMBOL_H_
