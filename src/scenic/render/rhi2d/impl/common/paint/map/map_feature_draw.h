// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_FEATURE_DRAW_H_
#define SCENIC_RHI2D_MAP_FEATURE_DRAW_H_

#include "gis/envelope.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/map/map_feature_prep.h"

class OGRFeature;
class OGRGeometry;

namespace scenic {
namespace detail {

int draw_unprepared_feature(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                            const RenderOptions2d* options, OGRFeature* feature,
                            int op, const PrepFieldCache* fields,
                            const gis::Envelope* env_viewp);

int draw_unprepared_geometry(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                             const RenderOptions2d* options,
                             const OGRGeometry* geom, const Style* style,
                             int op, const gis::Envelope* env_viewp);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_FEATURE_DRAW_H_
