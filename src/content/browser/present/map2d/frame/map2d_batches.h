// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/document/map_scene.h"
#include "gis/vista/frame/frame.h"
#include "ogrsf_frmts.h"

namespace content {
namespace detail {

// Owns OGR geometries whose pointers LayerBatch keeps until layout returns.
struct Map2dBatches {
  std::vector<gis::vista::LayerBatch> batches;
  std::vector<std::unique_ptr<OGRGeometry>> owned;
};

// Visible layers in CRS84 lon/lat. Stored map Y is -lat.
// use_carto_slots: default style document. A layer whose name is already a
// carto source-layer stays intact; other layers are split by geometry.
// |scale| > 0 applies D2 filters (label importance, line visibility).
Map2dBatches visible_layer_batches(const std::vector<MapScene::Layer>& layers,
                                   bool use_carto_slots, double scale = 0.0);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_BATCHES_H_
