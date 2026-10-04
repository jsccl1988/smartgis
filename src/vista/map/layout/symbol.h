// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Layout policy for symbol icons and text labels (collision + emit).

#ifndef GIS_VISTA_FRAME_DETAIL_LAYOUT_SYMBOL_H_
#define GIS_VISTA_FRAME_DETAIL_LAYOUT_SYMBOL_H_

#include <vector>

#include "vista/map/collision.h"
#include "vista/map/ir.h"

namespace vista {
namespace detail {

void emit_symbols(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, float fblc,
                  LabelGrid* grid, MapIR* frame);

}  // namespace detail
}  // namespace vista

#endif  // GIS_VISTA_FRAME_DETAIL_LAYOUT_SYMBOL_H_
