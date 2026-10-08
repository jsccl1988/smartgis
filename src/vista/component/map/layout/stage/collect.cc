// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/layout/collect.h"

#include "gis/style/eval/style_rules.h"

namespace vista {
namespace detail {

std::vector<const gis::style::StyleLayer*> collect_visible(
    const std::vector<gis::style::StyleLayer>& layers, double zoom) {
  std::vector<const gis::style::StyleLayer*> visible;
  visible.reserve(layers.size());
  for (const gis::style::StyleLayer& layer : layers) {
    if (gis::style::layer_matches_zoom(layer, zoom)) {
      visible.push_back(&layer);
    }
  }
  return visible;
}

}  // namespace detail
}  // namespace vista
