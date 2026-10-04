// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Merge adjacent same-style DrawItems so one GPU batch covers many geoms.

#ifndef VISTA_MAP_LAYOUT_COALESCE_H_
#define VISTA_MAP_LAYOUT_COALESCE_H_

#include "vista/map/draw.h"

namespace vista {
namespace detail {

void coalesce_draw_items(std::vector<DrawItem>* items);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MAP_LAYOUT_COALESCE_H_
