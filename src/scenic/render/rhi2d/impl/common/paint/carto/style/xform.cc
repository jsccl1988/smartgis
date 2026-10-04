// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/style/xform.h"

#include "base/math/affine2.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"

namespace scenic {
namespace detail {

namespace {

bool is_degenerate_ports(const RenderContext& rc) {
  return is_equal(rc.windowport.m_fWWidth, 0, dEPSILON) &&
         is_equal(rc.windowport.m_fWHeight, 0, dEPSILON) &&
         is_equal(rc.viewport.m_fVWidth, 0, dEPSILON) &&
         is_equal(rc.viewport.m_fVHeight, 0, dEPSILON);
}

}  // namespace

void Rhi2dCartoDrawXform::refresh_cache() const {
  if (!rc_) {
    cached_ = LpToDp2{};
    cache_valid_ = false;
    return;
  }
  const float wox = rc_->windowport.m_fWOX;
  const float woy = rc_->windowport.m_fWOY;
  const float vox = rc_->viewport.m_fVOX;
  const float voy = rc_->viewport.m_fVOY;
  const float scale = rc_->fblc;
  const float view_h = rc_->viewport.m_fVHeight;
  if (cache_valid_ && wox == fp_wox_ && woy == fp_woy_ && vox == fp_vox_ &&
      voy == fp_voy_ && scale == fp_scale_ && view_h == fp_view_h_) {
    return;
  }
  cached_ = make_lp_to_dp(*rc_);
  fp_wox_ = wox;
  fp_woy_ = woy;
  fp_vox_ = vox;
  fp_voy_ = voy;
  fp_scale_ = scale;
  fp_view_h_ = view_h;
  cache_valid_ = true;
}

const LpToDp2& Rhi2dCartoDrawXform::lp_to_dp2() const {
  refresh_cache();
  return cached_;
}

int Rhi2dCartoDrawXform::lp_to_dp(float x, float y, long& X, long& Y) const {
  if (!rc_ || is_degenerate_ports(*rc_)) {
    X = static_cast<long>(x);
    Y = static_cast<long>(y);
    return SMT_ERR_FAILURE;
  }

  transform_xy(lp_to_dp2(), x, y, &X, &Y);
  return SMT_ERR_NONE;
}

int Rhi2dCartoDrawXform::dp_to_lp(LONG X, LONG Y, float& x, float& y) const {
  if (!rc_ || is_degenerate_ports(*rc_)) {
    x = static_cast<float>(X);
    y = static_cast<float>(Y);
    return SMT_ERR_FAILURE;
  }

  inverse_xy(lp_to_dp2(), X, Y, &x, &y);
  return SMT_ERR_NONE;
}

int Rhi2dCartoDrawXform::lrect_to_drect(const fRect& frect, lRect& lrect) const {
  lp_to_dp(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  lp_to_dp(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);

  return SMT_ERR_NONE;
}

int Rhi2dCartoDrawXform::drect_to_lrect(const lRect& lrect, fRect& frect) const {
  dp_to_lp(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  dp_to_lp(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);

  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace scenic
