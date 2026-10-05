// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Visible style layers for one Layout::build (zoom filter only).

#ifndef VISTA_COMPONENT_MAP_LAYOUT_COLLECT_H_
#define VISTA_COMPONENT_MAP_LAYOUT_COLLECT_H_

#include <vector>

#include "gis/style/style_types.h"

namespace vista {
namespace detail {

std::vector<const gis::style::StyleLayer*> collect_visible(
    const std::vector<gis::style::StyleLayer>& layers, double zoom);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_LAYOUT_COLLECT_H_
