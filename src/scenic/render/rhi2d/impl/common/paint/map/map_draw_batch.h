// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_DRAW_BATCH_H_
#define SCENIC_RHI2D_MAP_DRAW_BATCH_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_prep.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "ogrsf_frmts.h"

namespace scenic {
namespace detail {

using FeatureFallbackFn = std::function<void(OGRFeature*, int)>;

// GeomOnly reserved for a future parallel HDC draw path.
enum class DrawBatchMode : uint8_t { All, GeomOnly };

// Playback of CPU-prepped features through carto_draw. Coalesces plain
// lines/roads into PolyPolyline and consecutive same-fill polygons under
// one style session. Play emits draw_device_* only (no LP→DP).
void draw_prepared_batch(Rhi2dCartoDraw* carto_draw, int op,
                         std::vector<OGRFeature*>* feats,
                         std::vector<PreparedFeature>* prepared,
                         const FeatureFallbackFn& fallback,
                         DrawBatchMode mode);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_DRAW_BATCH_H_
