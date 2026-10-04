// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_GDI_PREVIEW_XFORM_H_
#define LEGACY_RENDER_GDI_PREVIEW_XFORM_H_

#include <algorithm>
#include <cmath>

#include "scenic/detail/viewport.h"

namespace scenic {
namespace detail {

// Cap StretchBlt dest relative to the device viewport. Unbounded accumulate
// (wheel spam before settle) made Refresh/OnDraw hang the UI thread.
inline constexpr float kPreviewStretchMinScale = 0.25f;
inline constexpr float kPreviewStretchMaxScale = 4.f;

// MapLibre-style interactive zoom preview: scale |dest| around device-pixel
// anchor |org| by 1/fscale. Matches PreviewZoomScale windowport math
// (windowport *= fscale, fblc /= fscale): fscale < 1 zooms in (bitmap grows).
inline bool apply_preview_zoom_stretch(base::Viewport* dest, float org_x,
                                       float org_y, float fscale) {
  if (!dest || !(fscale > 0.f) || !std::isfinite(fscale)) {
    return false;
  }
  const float sox = dest->m_fVOX;
  const float soy = dest->m_fVOY;
  const float sw = dest->m_fVWidth;
  const float sh = dest->m_fVHeight;
  if (!(sw > 0.f) || !(sh > 0.f) || !std::isfinite(sw) || !std::isfinite(sh)) {
    return false;
  }
  dest->m_fVWidth = sw / fscale;
  dest->m_fVHeight = sh / fscale;
  dest->m_fVOX = org_x - (org_x - sox) / fscale;
  dest->m_fVOY = org_y - (org_y - soy) / fscale;
  return true;
}

// Recompute dest stretch from the last published front (|src_vp| 1:1) using
// painted vs current world scale. Avoids accumulating vir_viewport2 across
// many PreviewZoomScale ticks (pathological StretchBlt).
inline bool set_preview_stretch_from_fblc(base::Viewport* dest,
                                          const base::Viewport& src_vp,
                                          float painted_fblc, float cur_fblc,
                                          float org_x, float org_y) {
  if (!dest || !(painted_fblc > 0.f) || !(cur_fblc > 0.f) ||
      !std::isfinite(painted_fblc) || !std::isfinite(cur_fblc)) {
    return false;
  }
  float scale = cur_fblc / painted_fblc;
  scale = (std::max)(kPreviewStretchMinScale,
                     (std::min)(kPreviewStretchMaxScale, scale));
  // apply_preview_zoom_stretch uses dest /= fscale → fscale = 1/scale.
  *dest = src_vp;
  return apply_preview_zoom_stretch(dest, org_x, org_y, 1.f / scale);
}

// Clamp an already-built dest rect so Refresh cannot StretchBlt unbounded.
inline void clamp_preview_dest(base::Viewport* dest,
                               const base::Viewport& device_vp) {
  if (!dest || !(device_vp.m_fVWidth > 0.f) || !(device_vp.m_fVHeight > 0.f)) {
    return;
  }
  const float max_w = device_vp.m_fVWidth * kPreviewStretchMaxScale;
  const float max_h = device_vp.m_fVHeight * kPreviewStretchMaxScale;
  if (dest->m_fVWidth > max_w || dest->m_fVHeight > max_h ||
      dest->m_fVWidth < device_vp.m_fVWidth * kPreviewStretchMinScale ||
      dest->m_fVHeight < device_vp.m_fVHeight * kPreviewStretchMinScale ||
      !std::isfinite(dest->m_fVWidth) || !std::isfinite(dest->m_fVHeight) ||
      !std::isfinite(dest->m_fVOX) || !std::isfinite(dest->m_fVOY)) {
    *dest = device_vp;
  }
}

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_GDI_PREVIEW_XFORM_H_
