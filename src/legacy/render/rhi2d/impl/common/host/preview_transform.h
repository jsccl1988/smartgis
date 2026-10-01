// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_HOST_PREVIEW_TRANSFORM_H_
#define LEGACY_RENDER_RHI2D_HOST_PREVIEW_TRANSFORM_H_

#include <cmath>

#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/preview_xform.h"

namespace render {
namespace detail {

// Scale world windowport and fblc for interactive zoom (origin adjusted by
// caller after a second DP→LP sample at the same device pixel).
inline bool scale_windowport_zoom(base::Windowport* wp, float* fblc,
                                  float fscale) {
  if (!wp || !fblc || !(fscale > 0.f) || !std::isfinite(fscale)) {
    return false;
  }
  wp->m_fWHeight *= fscale;
  wp->m_fWWidth *= fscale;
  *fblc /= fscale;
  return true;
}

inline void nudge_windowport_origin(base::Windowport* wp, float dx_world,
                                    float dy_world) {
  if (!wp) {
    return;
  }
  wp->m_fWOX -= dx_world;
  wp->m_fWOY -= dy_world;
}

// Rebuild StretchBlt preview viewports from the last published baseline.
// |vir1| = published source (device viewport); |vir2| = stretch dest.
inline void rebuild_preview_viewports(base::Viewport* vir1,
                                      base::Viewport* vir2,
                                      const base::Viewport& device_vp,
                                      bool has_painted_baseline,
                                      float painted_fblc, float cur_fblc,
                                      float org_x, float org_y) {
  if (!vir1 || !vir2) {
    return;
  }
  *vir1 = device_vp;
  if (!has_painted_baseline || !(painted_fblc > 0.f)) {
    *vir2 = device_vp;
  } else if (!set_preview_stretch_from_fblc(vir2, device_vp, painted_fblc,
                                            cur_fblc, org_x, org_y)) {
    *vir2 = device_vp;
  }
  clamp_preview_dest(vir2, device_vp);
}

// 1:1 preview while an async settle runs (no stretch).
inline void reset_preview_viewports_identity(base::Viewport* vir1,
                                             base::Viewport* vir2,
                                             const base::Viewport& device_vp) {
  if (!vir1 || !vir2) {
    return;
  }
  *vir1 = device_vp;
  *vir2 = device_vp;
}

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_HOST_PREVIEW_TRANSFORM_H_
