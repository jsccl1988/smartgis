// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_GDI_CANVAS_XFORM_H_
#define SCENIC_GDI_CANVAS_XFORM_H_

#include "base/math/affine2.h"
#include "scenic/detail/err.h"
#include "scenic/detail/geom.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"

using namespace base;

namespace scenic {
namespace detail {

// LP↔DP helpers bound to a non-owning RenderContext.
// Caches LpToDp2 for the current windowport/viewport/fblc fingerprint.
class Rhi2dCartoDrawXform {
 public:
  Rhi2dCartoDrawXform() = default;

  void set_context(RenderContext* rc) {
    rc_ = rc;
    cache_valid_ = false;
  }
  RenderContext* context() const { return rc_; }

  // Cached map; empty/degenerate context returns identity-ish scale=1.
  const LpToDp2& lp_to_dp2() const;

  int lp_to_dp(float x, float y, long& X, long& Y) const;
  int dp_to_lp(LONG X, LONG Y, float& x, float& y) const;
  int lrect_to_drect(const fRect& frect, lRect& lrect) const;
  int drect_to_lrect(const lRect& lrect, fRect& frect) const;

 private:
  void refresh_cache() const;

  RenderContext* rc_ = nullptr;
  mutable LpToDp2 cached_{};
  mutable bool cache_valid_ = false;
  mutable float fp_wox_ = 0.f;
  mutable float fp_woy_ = 0.f;
  mutable float fp_vox_ = 0.f;
  mutable float fp_voy_ = 0.f;
  mutable float fp_scale_ = 0.f;
  mutable float fp_view_h_ = 0.f;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_GDI_CANVAS_XFORM_H_
