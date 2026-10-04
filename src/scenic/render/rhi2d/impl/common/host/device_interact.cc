// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "scenic/render/rhi2d/impl/common/host/render_device.h"

#include <mutex>

#include "scenic/render/rhi2d/impl/common/cc/layer_tree_host.h"
#include "scenic/render/rhi2d/impl/common/host/preview_transform.h"

using namespace gis;
using namespace base;

namespace scenic {
namespace detail {

int Rhi2dRenderDevice::LPToDP(float x, float y, LONG &X, LONG &Y) const {
  if (detail::ports_are_all_zero(m_Viewport, m_Windowport)) {
    X = x;
    Y = y;
    return SMT_ERR_FAILURE;
  }

  const LpToDp2 a = detail::make_lp_to_dp(m_Viewport, m_Windowport, m_fblc);
  transform_xy(a, x, y, &X, &Y);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::DPToLP(LONG X, LONG Y, float &x, float &y) const {
  if (detail::ports_are_all_zero(m_Viewport, m_Windowport)) {
    x = X;
    y = Y;
    return SMT_ERR_FAILURE;
  }

  const LpToDp2 a = detail::make_lp_to_dp(m_Viewport, m_Windowport, m_fblc);
  inverse_xy(a, X, Y, &x, &y);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::LRectToDRect(const fRect &frect, lRect &lrect) const {
  LPToDP(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  LPToDP(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::DRectToLRect(const lRect &lrect, fRect &frect) const {
  DPToLP(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  DPToLP(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Refresh() {
  // Teardown / closed HWND -- never touch shared front.
  if (!m_hWnd || !::IsWindow(m_hWnd) || m_leak_on_close_) {
    return SMT_ERR_FAILURE;
  }
  // Never block or race the map worker from the UI thread. Preview pan/zoom
  // already updated the viewport; present once the pending frame finishes.
  if (layer_tree_host_ && layer_tree_host_->is_busy()) {
    present_.arm_present();
  }

  // Compose once in OnDraw -- RenderMapToDC. Doing clear+3 blits here AND
  // again in RenderMapToDC doubled Debug color-key cost (~0.8 FPS pan).
  // Direct GetDC BitBlt is discarded by DWM; InvalidateRect is required.
  invalidate_map_present();
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::Refresh(const Map *pMap, fRect frect) {
  lRect lrect;
  LRectToDRect(frect, lrect);
  RefreshDirectly(pMap, lrect);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::RefreshDirectly(const Map *pSmtMap, lRect rect,
                                          bool bRealTime) {
  (void)rect;
  return rerender_map(pSmtMap, bRealTime);
}

int Rhi2dRenderDevice::ZoomMove(const Map *pSmtMap, fPoint dbfPointOffset,
                                   bool bRealTime) {
  // MapLibre-style: never block the UI on the FrameJob. Preview + async settle.
  detail::nudge_windowport_origin(&m_Windowport, dbfPointOffset.x,
                                  dbfPointOffset.y);
  return rerender_map(pSmtMap, bRealTime);
}

int Rhi2dRenderDevice::ZoomScale(const Map *pSmtMap, lPoint orgPoint,
                                    float fscale, bool bRealTime) {
  if (PreviewZoomScale(orgPoint, fscale) != SMT_ERR_NONE) {
    return SMT_ERR_INVALID_PARAM;
  }
  return rerender_map(pSmtMap, bRealTime);
}

void Rhi2dRenderDevice::note_painted_preview_baseline() {
  painted_windowport_ = m_Windowport;
  painted_fblc_ = m_fblc;
  has_painted_preview_ = (m_fblc > 0.0);
}

uint64_t Rhi2dRenderDevice::map_published_generation() const {
  return layer_tree_host_ ? layer_tree_host_->published_generation()
                          : uint64_t{0};
}

void Rhi2dRenderDevice::apply_stretch_preview(float org_x, float org_y) {
  // Always refresh stretch preview. try_lock-skip left vir_viewport2_
  // stale during worker publish -- wheel looked like a no-op (zoom_gate
  // pixel_diff=0). Brief lock only; hang was wait_idle + sync encode.
  //
  // First china bootstrap (!has_painted_preview_): preview is identity and
  // no worker has published yet. Locking shared_front_mu_ here aborted with
  // MSVC "unlock of unowned mutex" in gdi_map_paint_test (matrix). Skip the
  // lock until a baseline exists; wheel/Zoom after publish still serialize.
  const auto rebuild = [&]() {
    detail::rebuild_preview_viewports(
        &vir_viewport1_, &vir_viewport2_, m_Viewport, has_painted_preview_,
        static_cast<float>(painted_fblc_), static_cast<float>(m_fblc), org_x,
        org_y);
    // Pan-slide offset is orthogonal; clear so Zoom Stretch owns the preview.
    m_curDrawingOrg.x = 0;
    m_curDrawingOrg.y = 0;
  };
  if (!has_painted_preview_) {
    rebuild();
    return;
  }
  std::lock_guard<std::mutex> front_lock(shared_front_mu_);
  rebuild();
}

bool Rhi2dRenderDevice::rubber_band_device_focus(const fRect &rect,
                                                    float *org_x,
                                                    float *org_y) const {
  if (!org_x || !org_y) {
    return false;
  }
  LONG x0 = 0;
  LONG y0 = 0;
  LONG x1 = 0;
  LONG y1 = 0;
  if (LPToDP(rect.lb.x, rect.lb.y, x0, y0) != SMT_ERR_NONE ||
      LPToDP(rect.rt.x, rect.rt.y, x1, y1) != SMT_ERR_NONE) {
    return false;
  }
  *org_x = 0.5f * static_cast<float>(x0 + x1);
  *org_y = 0.5f * static_cast<float>(y0 + y1);
  return true;
}

int Rhi2dRenderDevice::paint_map_bootstrap_sync(const Map *map) {
  if (!layer_tree_host_) {
    return SMT_ERR_FAILURE;
  }
  if (layer_tree_host_->is_busy() || layer_tree_host_->has_pending()) {
    layer_tree_host_->cancel();
    (void)layer_tree_host_->wait_idle(800);
  }
  {
    RenderContext sync_rc(
        m_Viewport, m_Windowport, static_cast<float>(m_fblc), map,
        static_cast<int>(m_Viewport.m_fVOX),
        static_cast<int>(m_Viewport.m_fVOY),
        static_cast<int>(m_Viewport.m_fVWidth),
        static_cast<int>(m_Viewport.m_fVHeight), R2_COPYPEN);
    if (layer_tree_host_->paint_map_sync(sync_rc, m_rdOptions) !=
        SMT_ERR_NONE) {
      return SMT_ERR_FAILURE;
    }
  }
  note_painted_preview_baseline();
  Refresh();
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::PreviewZoomScale(lPoint orgPoint, float fscale) {
  // MapLibre interactive zoom: update world windowport, then rebuild
  // vir_viewport2 from the last published baseline (never accumulate).
  if (!detail::is_valid_zoom_scale(fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }

  float x1, y1, x2, y2;
  DPToLP(orgPoint.x, orgPoint.y, x1, y1);
  if (!detail::scale_windowport_zoom(&m_Windowport, &m_fblc, fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }
  DPToLP(orgPoint.x, orgPoint.y, x2, y2);
  detail::nudge_windowport_origin(&m_Windowport, x2 - x1, y2 - y1);
  apply_stretch_preview(static_cast<float>(orgPoint.x),
                        static_cast<float>(orgPoint.y));
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::PreviewZoomMove(fPoint dbfPointOffset) {
  // World windowport only -- pixel slide uses SetCurDrawingOrg + BitBlt.
  // Do not touch vir_viewport1 (worker publish / Stretch source).
  detail::nudge_windowport_origin(&m_Windowport, dbfPointOffset.x,
                                  dbfPointOffset.y);
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::ZoomToRect(const Map *pSmtMap, fRect rect,
                                     bool bRealTime) {
  // Never Sleep-poll or sync-encode on the UI thread -- settle via worker
  // (urgent or debounced). Do not cancel() here: sticky cancel aborted the
  // first Edit china FrameJob -- blank white map. stage_map_job cancels when
  // it replaces an in-flight job.

  // Focus for StretchBlt preview must use the *old* windowport (rubber-band
  // device pixels). Mutating m_Windowport first made LP->DP map to the new
  // extent and reset_preview_viewports_identity left mouse-up showing the
  // previous full map until the FrameJob finished.
  float org_x = 0.f;
  float org_y = 0.f;
  detail::viewport_device_center(m_Viewport, &org_x, &org_y);
  (void)rubber_band_device_focus(rect, &org_x, &org_y);

  if (!detail::fit_windowport_contain(&m_Windowport, &m_fblc, m_Viewport,
                                      rect.lb.x, rect.lb.y, rect.width(),
                                      rect.height())) {
    return SMT_ERR_INVALID_PARAM;
  }

  // MapLibre-like: stretch the last published front around the rubber-band
  // focus while the worker lands the new windowport. Do not reset
  // painted_fblc -- that baseline still matches the published bitmap until
  // Timer present refreshes it.
  apply_stretch_preview(org_x, org_y);

  if (bRealTime && !has_painted_preview_) {
    // First Edit fit / china bootstrap: sync host paint once so the map is
    // visible before return. Wheel uses PreviewZoomScale + async settle and
    // must not hit this path (would hang on china re-tessellate).
    return paint_map_bootstrap_sync(pSmtMap);
  }
  // Tool rubber-band / restore after a painted baseline: urgent settle so
  // mouse-up does not wait the ~200 ms debounce with a stale identity blit.
  return rerender_map(pSmtMap, bRealTime || has_painted_preview_);
}

int Rhi2dRenderDevice::rerender_map(const Map *map, bool realtime) {
  if (!map) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!realtime) {
    return ReRenderMapByProxy(map, m_Viewport.m_fVOX, m_Viewport.m_fVOY,
                              m_Viewport.m_fVWidth, m_Viewport.m_fVHeight);
  }
  return ReRenderMapRealTime(map, m_Viewport.m_fVOX, m_Viewport.m_fVOY,
                             m_Viewport.m_fVWidth, m_Viewport.m_fVHeight);
}

}  // namespace detail
}  // namespace scenic
