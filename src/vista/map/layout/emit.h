// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Painter dispatch for visible style layers. No RHI.

#ifndef VISTA_MAP_LAYOUT_EMIT_H_
#define VISTA_MAP_LAYOUT_EMIT_H_

#include <atomic>
#include <vector>

#include "vista/map/batch.h"
#include "vista/map/collision.h"
#include "vista/map/draw.h"
#include "vista/map/layout.h"

namespace gis {
namespace style {
struct StyleLayer;
}
}  // namespace gis

namespace vista {
namespace detail {

inline bool layout_gen_stale(const LayoutInput& in) {
  return in.live_layout_gen != nullptr && in.layout_gen != 0 &&
         in.live_layout_gen->load(std::memory_order_acquire) != in.layout_gen;
}

void emit_visible_layers(const LayoutInput& in,
                         const std::vector<LayerBatch>& layers,
                         const std::vector<const gis::style::StyleLayer*>& visible,
                         LabelGrid* grid, MapFrame* frame);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MAP_LAYOUT_EMIT_H_
