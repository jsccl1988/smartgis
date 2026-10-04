// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_VIEW_H_
#define SCENIC_RHI2D_MAP_VIEW_H_

#include "gis/envelope.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_api.h"

namespace scenic {
namespace detail {

// RGB(170, 211, 223) — ocean clear / layer color-key.
inline constexpr unsigned long kMapOceanClear = 170ul | (211ul << 8) | (223ul << 16);

// Device viewport → map envelope (one call per layer/map pass).
inline bool map_view_envelope(Rhi2dCartoDraw* carto, const RenderContext& ctx,
                              gis::Envelope* out) {
  if (!carto || !out) {
    return false;
  }
  lRect l_viewp;
  fRect f_viewp;
  viewport_to_rect(l_viewp, ctx.viewport);
  carto->drect_to_lrect(l_viewp, f_viewp);
  rect_to_envelope(*out, f_viewp);
  return true;
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_VIEW_H_
