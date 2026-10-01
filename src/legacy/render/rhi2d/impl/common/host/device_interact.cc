// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "legacy/render/rhi2d/impl/common/host/render_device.h"

#include <cmath>
#include <mutex>

#include "base/math/affine2.h"
#include "base/math/simd.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/common/host/preview_transform.h"
#include "legacy/render/rhi2d/impl/common/cc/layer_tree_host.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {
namespace {

LpToDp2 make_lp_to_dp(const Viewport &vp, const Windowport &wp, float fblc) {
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

}  // namespace

int SmtRhi2dRenderDevice::LPToDP(float x, float y, LONG &X, LONG &Y) const {
  if (is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVHeight, 0, dEPSILON)) {
    X = x;
    Y = y;

    return SMT_ERR_FAILURE;
  }

  const LpToDp2 a = make_lp_to_dp(m_Viewport, m_Windowport, m_fblc);
  transform_xy(a, x, y, &X, &Y);
  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::DPToLP(LONG X, LONG Y, float &x, float &y) const {
  if (is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVHeight, 0, dEPSILON)) {
    x = X;
    y = Y;

    return SMT_ERR_FAILURE;
  }

  const LpToDp2 a = make_lp_to_dp(m_Viewport, m_Windowport, m_fblc);
  inverse_xy(a, X, Y, &x, &y);
  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::LRectToDRect(const fRect &frect, lRect &lrect) const {
  LPToDP(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  LPToDP(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);

  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::DRectToLRect(const lRect &lrect, fRect &frect) const {
  DPToLP(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  DPToLP(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);

  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::Refresh() {
  // Teardown / closed HWND �?never touch shared front.
  if (!m_hWnd || !::IsWindow(m_hWnd) || m_leak_on_close_) {
    return SMT_ERR_FAILURE;
  }
  // Never block or race the map worker from the UI thread. Preview pan/zoom
  // already updated the viewport; present once the pending frame finishes.
  if (layer_tree_host_ && layer_tree_host_->is_busy()) {
    present_.arm_present();
  }

  // Compose once in OnDraw �?RenderMapToDC. Doing clear+3 blits here AND
  // again in RenderMapToDC doubled Debug color-key cost (~0.8 FPS pan).
  // Direct GetDC BitBlt is discarded by DWM; InvalidateRect is required.
  invalidate_map_present();
  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::Refresh(const SmtMap *pMap, fRect frect) {
  lRect lrect;
  LRectToDRect(frect, lrect);
  RefreshDirectly(pMap, lrect);

  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::RefreshDirectly(const SmtMap *pSmtMap, lRect rect,
                                        bool bRealTime) {
  (void)rect;
  return rerender_map(pSmtMap, bRealTime);
}

int SmtRhi2dRenderDevice::ZoomMove(const SmtMap *pSmtMap, fPoint dbfPointOffset,
                                 bool bRealTime) {
  // MapLibre-style: never block the UI on the FrameJob. Preview + async settle.
  m_Windowport.m_fWOX -= dbfPointOffset.x;
  m_Windowport.m_fWOY -= dbfPointOffset.y;
  return rerender_map(pSmtMap, bRealTime);
}

int SmtRhi2dRenderDevice::ZoomScale(const SmtMap *pSmtMap, lPoint orgPoint,
                                  float fscale, bool bRealTime) {
  if (!(fscale > 0.f) || !std::isfinite(fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Preview stretch under the shared-front lock, then async settle. Do not
  // wait_idle �?that made wheel/tool zoom hitch while a FrameJob ran.
  if (PreviewZoomScale(orgPoint, fscale) != SMT_ERR_NONE) {
    return SMT_ERR_INVALID_PARAM;
  }
  return rerender_map(pSmtMap, bRealTime);
}

void SmtRhi2dRenderDevice::note_painted_preview_baseline() {
  painted_windowport_ = m_Windowport;
  painted_fblc_ = m_fblc;
  has_painted_preview_ = (m_fblc > 0.0);
}

int SmtRhi2dRenderDevice::PreviewZoomScale(lPoint orgPoint, float fscale) {
  // MapLibre interactive zoom: update world windowport, then rebuild
  // vir_viewport2 from the last published baseline (never accumulate).
  if (!(fscale > 0.f) || !std::isfinite(fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }

  float x1, y1, x2, y2;
  DPToLP(orgPoint.x, orgPoint.y, x1, y1);
  if (!detail::scale_windowport_zoom(&m_Windowport, &m_fblc, fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }
  DPToLP(orgPoint.x, orgPoint.y, x2, y2);
  detail::nudge_windowport_origin(&m_Windowport, x2 - x1, y2 - y1);

  {
    // Always refresh stretch preview. try_lock-skip left vir_viewport2_
    // stale during worker publish — wheel looked like a no-op (zoom_gate
    // pixel_diff=0). Brief lock only; hang was wait_idle + sync encode.
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    detail::rebuild_preview_viewports(
        &vir_viewport1_, &vir_viewport2_, m_Viewport, has_painted_preview_,
        static_cast<float>(painted_fblc_), static_cast<float>(m_fblc),
        static_cast<float>(orgPoint.x), static_cast<float>(orgPoint.y));
  }
  // Pan-slide offset is orthogonal; clear so Zoom Stretch owns the preview.
  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;
  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::PreviewZoomMove(fPoint dbfPointOffset) {
  // World windowport only — pixel slide uses SetCurDrawingOrg + BitBlt.
  // Do not touch vir_viewport1 (worker publish / Stretch source).
  detail::nudge_windowport_origin(&m_Windowport, dbfPointOffset.x,
                                  dbfPointOffset.y);
  return SMT_ERR_NONE;
}

int SmtRhi2dRenderDevice::ZoomToRect(const SmtMap *pSmtMap, fRect rect,
                                   bool bRealTime) {
  // Never Sleep-poll or sync-encode on the UI thread �?settle via worker
  // (urgent or debounced). Do not cancel() here: sticky cancel aborted the
  // first Edit china FrameJob �?blank white map. stage_map_job cancels when
  // it replaces an in-flight job.

  m_Windowport.m_fWOX = rect.lb.x;
  m_Windowport.m_fWOY = rect.lb.y;
  m_Windowport.m_fWHeight = rect.height();
  m_Windowport.m_fWWidth = rect.width();

  if (is_equal(m_Viewport.m_fVWidth, 0, dEPSILON) ||
      is_equal(m_Viewport.m_fVHeight, 0, dEPSILON) ||
      is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) ||
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON)) {
    return SMT_ERR_INVALID_PARAM;
  }

  float xblc, yblc;
  xblc = m_Viewport.m_fVWidth / m_Windowport.m_fWWidth;
  yblc = m_Viewport.m_fVHeight / m_Windowport.m_fWHeight;

  m_fblc = (xblc > yblc) ? yblc : xblc;

  if (xblc < yblc) {
    m_Windowport.m_fWHeight = rect.height() * yblc / xblc;
  } else {
    m_Windowport.m_fWWidth = rect.width() * xblc / yblc;
  }

  // Keep last-good front as a 1:1 preview while the worker lands the new
  // windowport. Do not reset painted_fblc here — that baseline still matches
  // the published bitmap until Timer present refreshes it.
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    detail::reset_preview_viewports_identity(&vir_viewport1_, &vir_viewport2_,
                                             m_Viewport);
  }
  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;

  if (bRealTime) {
    // First Edit fit / china bootstrap: sync host paint once so the map is
    // visible before return. Wheel uses PreviewZoomScale + async settle and
    // must not hit this path (would hang on china re-tessellate).
    if (!has_painted_preview_) {
      if (!layer_tree_host_) {
        return SMT_ERR_FAILURE;
      }
      if (layer_tree_host_->is_busy() || layer_tree_host_->has_pending()) {
        layer_tree_host_->cancel();
        (void)layer_tree_host_->wait_idle(800);
      }
      {
        SmtRenderContext sync_rc(
            m_Viewport, m_Windowport, static_cast<float>(m_fblc), pSmtMap,
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
    return ReRenderMapRealTime(pSmtMap, static_cast<int>(m_Viewport.m_fVOX),
                               static_cast<int>(m_Viewport.m_fVOY),
                               static_cast<int>(m_Viewport.m_fVWidth),
                               static_cast<int>(m_Viewport.m_fVHeight));
  }
  return ReRenderMapByProxy(pSmtMap, static_cast<int>(m_Viewport.m_fVOX),
                            static_cast<int>(m_Viewport.m_fVOY),
                            static_cast<int>(m_Viewport.m_fVWidth),
                            static_cast<int>(m_Viewport.m_fVHeight));
}

int SmtRhi2dRenderDevice::rerender_map(const SmtMap *map, bool realtime) {
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

}  // namespace render
