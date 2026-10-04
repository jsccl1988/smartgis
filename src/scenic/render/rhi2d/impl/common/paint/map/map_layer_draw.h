// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_LAYER_DRAW_H_
#define SCENIC_RHI2D_MAP_LAYER_DRAW_H_

#include <functional>
#include <vector>

#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "gis/map/map.h"
#include "gis/tile/layer/provider_tile_layer.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_draw_batch.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_prep.h"

class OGRLayer;

namespace scenic {
namespace detail {

using FrameAbortFn = std::function<bool()>;

bool ogr_layer_envelope(OGRLayer* layer, gis::Envelope* out);

void collect_visible_map_layers(
    const gis::Map* map, const gis::Envelope& env_viewp,
    const FrameAbortFn& aborted, std::vector<OgrLayerBatch>* ogr_batches,
    std::vector<const gis::datasource::OgrRasterLayer*>* rasters,
    std::vector<const gis::tile::ProviderTileLayer*>* tiles);

void prepare_ogr_batches(std::vector<OgrLayerBatch>* ogr_batches,
                         const gis::Envelope& env_viewp, const LpToDp2& xform,
                         float fblc);

void encode_ogr_batch(Rhi2dCartoDraw* carto, OgrLayerBatch* batch, int op,
                      const FeatureFallbackFn& fallback);

int paint_ogr_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                    const RenderOptions2d* options, OGRLayer* layer, int op,
                    const FeatureFallbackFn& fallback);

int paint_raster_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                       const gis::datasource::OgrRasterLayer* layer);

int paint_tile_layer(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                     const gis::tile::ProviderTileLayer* layer);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_LAYER_DRAW_H_
