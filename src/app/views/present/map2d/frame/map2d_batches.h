// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PRESENT_MAP2D_BATCHES_H_
#define APP_VIEWS_PRESENT_MAP2D_BATCHES_H_

#include <memory>
#include <string>
#include <vector>

#include "app/views/document/map_scene.h"
#include "gis/vista/frame/frame.h"
#include "ogrsf_frmts.h"

namespace app {
namespace detail {

// Owns OGR geometries whose pointers LayerBatch keeps until layout returns.
struct Map2dBatches {
  std::vector<gis::vista::LayerBatch> batches;
  std::vector<std::unique_ptr<OGRGeometry>> owned;
};

// Visible layers in CRS84 lon/lat. Stored map Y is -lat.
// use_carto_slots: default style document. A layer whose name is already a
// carto source-layer stays intact; other layers are split by geometry.
Map2dBatches visible_layer_batches(const std::vector<MapScene::Layer>& layers,
                                   bool use_carto_slots);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_PRESENT_MAP2D_BATCHES_H_
