// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_

#include <vector>

#include "content/browser/document/map_scene.h"
#include "vista/frame/frame.h"

namespace content {
namespace detail {

// Copies MapScene layers into POD features (Y flipped to +lat, name fields
// normalized to UTF-8) and calls vista::build_layer_batches.
// use_carto_slots: default style document. |scale| > 0 applies D2 filters.
vista::LayerBatchSet visible_layer_batches(
    const std::vector<MapScene::Layer>& layers, bool use_carto_slots,
    double scale = 0.0);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_
