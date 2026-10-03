// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_HOST_PREVIEW_TRANSFORM_H_
#define LEGACY_RENDER_RHI2D_HOST_PREVIEW_TRANSFORM_H_

#include <cmath>

#include "base/math/affine2.h"
#include "legacy/gis/present/carto/style_bas_struct.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/preview_xform.h"

namespace render {
namespace detail {

inline constexpr float kPortEpsilon = 1e-5f;

inline bool dim_near_zero(float v) { return std::fabs(v) < kPortEpsilon; }

inline bool is_valid_zoom_scale(float fscale) {
  return (fscale > 0.f) && std::isfinite(fscale);
}

// LPToDP identity fallback: leftover ABI treats all-zero ports as unmapped.
inline bool ports_are_all_zero(const base::Viewport& vp,
                               const base::Windowport& wp) {
  return dim_near_zero(wp.m_fWWidth) && dim_near_zero(wp.m_fWHeight) &&
         dim_near_zero(vp.m_fVWidth) && dim_near_zero(vp.m_fVHeight);
}

inline bool ports_have_area(const base::Viewport& vp,
                            const base::Windowport& wp) {
  return !dim_near_zero(vp.m_fVWidth) && !dim_near_zero(vp.m_fVHeight) &&
         !dim_near_zero(wp.m_fWWidth) && !dim_near_zero(wp.m_fWHeight);
}

inline LpToDp2 make_lp_to_dp(const base::Viewport& vp,
                             const base::Windowport& wp, float fblc) {
  LpToDp2 a;
  a.wox = wp.m_fWOX;
  a.woy = wp.m_fWOY;
  a.vox = vp.m_fVOX;
  a.voy = vp.m_fVOY;
  a.scale = fblc;
  a.view_h = vp.m_fVHeight;
  a.flip_y = true;
  return a;
}

inline void viewport_device_center(const base::Viewport& vp, float* x,
                                   float* y) {
  if (x) {
    *x = vp.m_fVOX + vp.m_fVWidth * 0.5f;
  }
  if (y) {
    *y = vp.m_fVOY + vp.m_fVHeight * 0.5f;
  }
}

// Contain-fit |rect| into |vp|: write origin/size, then grow the short axis
// so world aspect matches the device viewport. Mutates |wp| even on failure
// (matches leftover ZoomToRect ABI).
inline bool fit_windowport_contain(base::Windowport* wp, float* fblc,
                                   const base::Viewport& vp, float lb_x,
                                   float lb_y, float width, float height) {
  if (!wp || !fblc) {
    return false;
  }
  wp->m_fWOX = lb_x;
  wp->m_fWOY = lb_y;
  wp->m_fWWidth = width;
  wp->m_fWHeight = height;
  if (!ports_have_area(vp, *wp)) {
    return false;
  }
  const float xblc = vp.m_fVWidth / wp->m_fWWidth;
  const float yblc = vp.m_fVHeight / wp->m_fWHeight;
  *fblc = (xblc > yblc) ? yblc : xblc;
  if (xblc < yblc) {
    wp->m_fWHeight = height * yblc / xblc;
  } else {
    wp->m_fWWidth = width * xblc / yblc;
  }
  return true;
}

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
