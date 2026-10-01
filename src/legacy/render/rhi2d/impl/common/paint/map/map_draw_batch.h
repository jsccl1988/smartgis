// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI2D_MAP_DRAW_BATCH_H_
#define SMT_LEGACY_RENDER_RHI2D_MAP_DRAW_BATCH_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "legacy/render/rhi2d/impl/common/paint/map/map_feature_prep.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "ogrsf_frmts.h"

namespace render {
namespace detail {

using FeatureFallbackFn = std::function<void(OGRFeature*, int)>;

// GeomOnly reserved for a future parallel HDC draw path.
enum class DrawBatchMode : uint8_t { All, GeomOnly };

// Draw a prepared feature batch through carto_draw (line/road coalesce).
void draw_prepared_batch(Rhi2dCartoDraw* carto_draw, int op,
                         std::vector<OGRFeature*>* feats,
                         std::vector<PreparedFeature>* prepared,
                         const FeatureFallbackFn& fallback,
                         DrawBatchMode mode);

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI2D_MAP_DRAW_BATCH_H_
