// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "legacy/render/rhi2d/impl/gdi/host/render_device.h"

#include <math.h>

#include <cmath>
#include <mutex>

#include "base/core/log.h"
#include "base/math/affine2.h"
#include "base/math/simd.h"
#include "base/trace/event/process_trace.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/preview_xform.h"
#include "legacy/render/rhi2d/impl/gdi/paint/player/gdi_player.h"
#include "legacy/render/rhi2d/impl/gdi/paint/gdiplus/gdiplus.h"
#include "legacy/render/rhi2d/impl/gdi/worker/render_thread.h"

using namespace gis;
using namespace base;
using namespace geo;

namespace render {

namespace {

constexpr int kGdiWorkerIdleTimeoutMs = 500;

// Cancel the in-flight FrameJob and wait up to timeout_ms. On timeout the
// caller keeps the last published front frame instead of spinning forever.
bool wait_gdi_worker_idle(SmtGdiRenderThread *worker,
                          int timeout_ms = kGdiWorkerIdleTimeoutMs) {
  if (!worker) {
    return true;
  }
  if (worker->is_busy() || worker->has_pending()) {
    worker->cancel();
  }
  return worker->wait_idle(timeout_ms);
}

// End the host encoder pass and publish last_pass_; emit gdi.encode spans.
void finish_host_encode_pass(detail::GdiCommandEncoder &encoder,
                             GdiCommandBuffer &last_pass) {
  if (!encoder.is_recording()) {
    return;
  }
  {
    BASE_TRACE_EVENT("end_pass", "gdi.encode");
    encoder.end_pass();
  }
  {
    BASE_TRACE_EVENT("take_buffer", "gdi.encode");
    last_pass = encoder.take_buffer();
  }
  if (base::trace::tracing_enabled()) {
    base::trace::process_trace().add_counter(
        "last_pass_ops", "gdi.encode", static_cast<int64_t>(last_pass.size()));
  }
}

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

int CreateRenderDevice(HINSTANCE hInst, LPRENDERDEVICE &pMrdDevice) {
  if (!pMrdDevice) {
    pMrdDevice = new SmtGdiRenderDevice(hInst);

    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}

int DestroyRenderDevice(LPRENDERDEVICE &pMrdDevice) {
  if (!pMrdDevice) return SMT_ERR_FAILURE;

  // Release may detach a still-running worker that holds Viewport& into this
  // device. Deleting then UAFs on close  �?leak until process exit instead.
  auto *gdi = static_cast<SmtGdiRenderDevice *>(pMrdDevice);
  if (gdi->release_may_leak()) {
    pMrdDevice = nullptr;
    return SMT_ERR_NONE;
  }

  SMT_SAFE_DELETE(pMrdDevice);

  return SMT_ERR_NONE;
}

SmtGdiRenderDevice::SmtGdiRenderDevice(HINSTANCE hInst)
    : SmtRenderDevice(hInst),
      ui_controller_(this),
      image_io_(this),
      paint_canvas_(hInst),
      layer_painter_(&paint_canvas_, &raster_back_, &map_front_,
                     &vir_viewport1_, &vir_viewport2_, &shared_front_mu_),
      render_thread_(NULL) {
  m_rBaseApi = RD_GDI;

  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;

  // Host sync paint: no FrameJob scheduler; canvas links device viewport.
  layer_painter_.set_scheduler(nullptr);
  layer_painter_.set_context(&host_rc_);
  layer_painter_.set_render_pra(&m_rdPra);

  render_thread_ = new SmtGdiRenderThread(hInst, vir_viewport1_, vir_viewport2_,
                                          &shared_front_mu_);
}

SmtGdiRenderDevice::~SmtGdiRenderDevice(void) {
  if (m_leak_on_close_) {
    // Worker still references Viewport members; do not Release/delete them.
    return;
  }
  Release();
}

bool SmtGdiRenderDevice::release_may_leak() {
  Release();
  return m_leak_on_close_;
}

int SmtGdiRenderDevice::Init(HWND hWnd, const char *logname) {
  if (hWnd == NULL || logname == NULL) return SMT_ERR_INVALID_PARAM;

  m_hWnd = hWnd;
  bind_rhi_present(hWnd);
  (void)gdiplus_ensure_started();

  LOGGING(LOG_INFO, "Init Gdi SmtRenderDevice ok!");

  m_strLogName = logname;

  compose_buf_.set_wnd(m_hWnd);
  map_front_.set_wnd(m_hWnd);
  dynamic_buf_.set_wnd(m_hWnd);
  raster_back_.set_wnd(m_hWnd);

  render_thread_->init(m_hWnd, logname);
  // Stay suspended until ReRenderMapByProxy/refresh. submit_frame here races
  // CreateNewFrame's nested pump and hangs --self-test after OnCreate.

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Destroy(void) {
  LOGGING(LOG_INFO, "Destroy Gdi SmtRenderDevice ok!");

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Release(void) {
  // Stop Timer present path before tearing down shared front / HWND.
  ui_controller_.m_bPresentPending = false;
  ui_controller_.m_bRedraw = false;
  ui_controller_.m_bUrgentSubmit = false;

  // Stop the worker before touching GDI objects it may still reference.
  bool worker_detached = false;
  bool worker_still_running = false;
  if (render_thread_) {
    worker_detached = render_thread_->shutdown();
    worker_still_running = worker_detached && !render_thread_->has_exited();
  }

  LOGGING(LOG_INFO, "Release Gdi SmtRenderDevice ok!");

  paint_canvas_.flush_style();

  // Detached worker still owns `this` briefly; deleting here UAFs on close.
  if (worker_detached) {
    render_thread_ = nullptr;
    if (worker_still_running) {
      m_leak_on_close_ = true;
    }
  } else {
    SMT_SAFE_DELETE(render_thread_);
  }
  // Invalidate HWND so late Refresh/Timer present cannot GetDC.
  m_hWnd = nullptr;
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Resize(int orgx, int orgy, int cx, int cy) {
  if (cx < 0 || cy < 0) return SMT_ERR_FAILURE;

  if (is_equal(m_Viewport.m_fVOX, orgx, dEPSILON) &&
      is_equal(m_Viewport.m_fVOY, orgy, dEPSILON) &&
      is_equal(m_Viewport.m_fVHeight, cy, dEPSILON) &&
      is_equal(m_Viewport.m_fVWidth, cx, dEPSILON)) {
    return SMT_ERR_FAILURE;
  }

  // Worker shared_front_ is share_from(map_front_). Reallocating the
  // host bitmap while a FrameJob still aliases the old HBITMAP leaves a
  // dangling front  �?black present and fail-fast on publish (0xC0000409).
  // Passive only  �?do not cancel(); sticky cancel aborted the next ZoomToRect.
  if (render_thread_) {
    for (int i = 0; i < 2000 && render_thread_->is_busy(); ++i) {
      ::Sleep(1);
    }
    if (render_thread_->is_busy()) {
      return SMT_ERR_FAILURE;
    }
  }

  m_Viewport.m_fVOX = orgx;
  m_Viewport.m_fVOY = orgy;
  m_Viewport.m_fVHeight = cy;
  m_Viewport.m_fVWidth = cx;

  vir_viewport1_ = m_Viewport;
  vir_viewport2_ = m_Viewport;

  if (is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) ||
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON)) {
    m_fblc = 1.f;
  } else {
    float xblc, yblc;
    xblc = m_Viewport.m_fVWidth / m_Windowport.m_fWWidth;
    yblc = m_Viewport.m_fVHeight / m_Windowport.m_fWHeight;
    m_fblc = (xblc > yblc) ? yblc : xblc;
  }

  // Do not HWND-present cleared buffers here  �?that flashed empty frames and
  // raced OnEraseBkgnd. Timer / ZoomToRect Refresh owns the first present.
  if (SMT_ERR_NONE ==
          compose_buf_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          dynamic_buf_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          map_front_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE ==
          raster_back_.set_size(m_Viewport.m_fVWidth, m_Viewport.m_fVHeight) &&
      SMT_ERR_NONE == render_thread_->resize(orgx, orgy, cx, cy, map_front_)) {
    return SMT_ERR_NONE;
  }

  return SMT_ERR_FAILURE;
}

int SmtGdiRenderDevice::LPToDP(float x, float y, LONG &X, LONG &Y) const {
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

int SmtGdiRenderDevice::DPToLP(LONG X, LONG Y, float &x, float &y) const {
  if (is_equal(m_Windowport.m_fWWidth, 0, dEPSILON) &&
      is_equal(m_Windowport.m_fWHeight, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVWidth, 0, dEPSILON) &&
      is_equal(m_Viewport.m_fVHeight, 0, dEPSILON)) {
    x = X;
    y = Y;

    return SMT_ERR_FAILURE;
  }

  Y = m_Viewport.m_fVHeight - Y;

  x = (X - m_Viewport.m_fVOX) / m_fblc + m_Windowport.m_fWOX;
  y = (Y - m_Viewport.m_fVOY) / m_fblc + m_Windowport.m_fWOY;

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::LRectToDRect(const fRect &frect, lRect &lrect) const {
  LPToDP(frect.lb.x, frect.lb.y, lrect.lb.x, lrect.lb.y);
  LPToDP(frect.rt.x, frect.rt.y, lrect.rt.x, lrect.rt.y);

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::DRectToLRect(const lRect &lrect, fRect &frect) const {
  DPToLP(lrect.lb.x, lrect.lb.y, frect.lb.x, frect.lb.y);
  DPToLP(lrect.rt.x, lrect.rt.y, frect.rt.x, frect.rt.y);

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Lock() {
  lock_.lock();
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Unlock() {
  lock_.unlock();
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Refresh() {
  // Teardown / closed HWND � never touch shared front.
  if (!m_hWnd || !::IsWindow(m_hWnd) || m_leak_on_close_) {
    return SMT_ERR_FAILURE;
  }
  // Never block or race the map worker from the UI thread. Preview pan/zoom
  // already updated the viewport; present once the pending frame finishes.
  if (render_thread_ && render_thread_->is_busy()) {
    ui_controller_.m_bPresentPending = true;
    // Still request paint so curDrawingOrg pan-slide of the last front shows.
    invalidate_map_present();
    return SMT_ERR_NONE;
  }

  // Compose under the shared-front lock only. Do not GetDC/present while
  // holding it � HWND sync messages re-enter OnDraw/Timer and deadlocked
  // on this non-recursive mutex (black map / hung ZoomToRect).
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    detail::clamp_preview_dest(&vir_viewport2_, m_Viewport);
    compose_buf_.clear(m_Viewport.m_fVOX, m_Viewport.m_fVOY,
                       m_Viewport.m_fVWidth, m_Viewport.m_fVHeight);

    map_front_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                       vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                       vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                       vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                       vir_viewport2_.m_fVHeight, BLT_STRETCH, SRCCOPY);

    dynamic_buf_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                         vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                         vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                         vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                         vir_viewport2_.m_fVHeight, BLT_TRANSPARENT, SRCCOPY);

    raster_back_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                         vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                         vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                         vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                         vir_viewport2_.m_fVHeight, BLT_TRANSPARENT, SRCCOPY);
  }

  // Show via WM_PAINT ? OnDraw ? RenderMapToDC(paint_dc). Direct GetDC BitBlt
  // is discarded by DWM / later validates, so the map stayed stale until a
  // mouse click forced another paint.
  invalidate_map_present();
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::Refresh(const SmtMap *pMap, fRect frect) {
  lRect lrect;
  LRectToDRect(frect, lrect);
  RefreshDirectly(pMap, lrect);

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::RefreshDirectly(const SmtMap *pSmtMap, lRect rect,
                                        bool bRealTime) {
  (void)rect;
  return rerender_map(pSmtMap, bRealTime);
}

int SmtGdiRenderDevice::ZoomMove(const SmtMap *pSmtMap, fPoint dbfPointOffset,
                                 bool bRealTime) {
  // MapLibre-style: never block the UI on the FrameJob. Preview + async settle.
  m_Windowport.m_fWOX -= dbfPointOffset.x;
  m_Windowport.m_fWOY -= dbfPointOffset.y;
  return rerender_map(pSmtMap, bRealTime);
}

int SmtGdiRenderDevice::ZoomScale(const SmtMap *pSmtMap, lPoint orgPoint,
                                  float fscale, bool bRealTime) {
  if (!(fscale > 0.f) || !std::isfinite(fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }

  // Preview stretch under the shared-front lock, then async settle. Do not
  // wait_idle � that made wheel/tool zoom hitch while a FrameJob ran.
  if (PreviewZoomScale(orgPoint, fscale) != SMT_ERR_NONE) {
    return SMT_ERR_INVALID_PARAM;
  }
  return rerender_map(pSmtMap, bRealTime);
}

void SmtGdiRenderDevice::note_painted_preview_baseline() {
  painted_windowport_ = m_Windowport;
  painted_fblc_ = m_fblc;
  has_painted_preview_ = (m_fblc > 0.0);
}

int SmtGdiRenderDevice::PreviewZoomScale(lPoint orgPoint, float fscale) {
  // MapLibre interactive zoom: update world windowport, then rebuild
  // vir_viewport2 from the last published baseline (never accumulate).
  if (!(fscale > 0.f) || !std::isfinite(fscale)) {
    return SMT_ERR_INVALID_PARAM;
  }

  float x1, y1, x2, y2;
  DPToLP(orgPoint.x, orgPoint.y, x1, y1);
  m_Windowport.m_fWHeight *= fscale;
  m_Windowport.m_fWWidth *= fscale;
  m_fblc /= fscale;
  DPToLP(orgPoint.x, orgPoint.y, x2, y2);
  m_Windowport.m_fWOX -= x2 - x1;
  m_Windowport.m_fWOY -= y2 - y1;

  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    // Source stays the published front (device viewport). Dest is rebuilt
    // from painted_fblc ? current fblc so wheel spam cannot explode size.
    vir_viewport1_ = m_Viewport;
    if (!has_painted_preview_ || !(painted_fblc_ > 0.0)) {
      note_painted_preview_baseline();
      vir_viewport2_ = m_Viewport;
    } else if (!detail::set_preview_stretch_from_fblc(
                   &vir_viewport2_, m_Viewport,
                   static_cast<float>(painted_fblc_),
                   static_cast<float>(m_fblc),
                   static_cast<float>(orgPoint.x),
                   static_cast<float>(orgPoint.y))) {
      vir_viewport2_ = m_Viewport;
    }
    detail::clamp_preview_dest(&vir_viewport2_, m_Viewport);
  }
  // Pan-slide offset is orthogonal; clear so Zoom Stretch owns the preview.
  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::PreviewZoomMove(fPoint dbfPointOffset) {
  // World windowport only � pixel slide uses SetCurDrawingOrg + BitBlt.
  // Do not touch vir_viewport1 (worker publish / Stretch source).
  m_Windowport.m_fWOX -= dbfPointOffset.x;
  m_Windowport.m_fWOY -= dbfPointOffset.y;
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::ZoomToRect(const SmtMap *pSmtMap, fRect rect,
                                   bool bRealTime) {
  if (!wait_gdi_worker_idle(render_thread_)) {
    return SMT_ERR_FAILURE;
  }

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

  // Discrete ZoomToRect: host sync paint while worker is idle. Interactive
  // PreviewZoom* stays MapLibre transform + async settle; this path must
  // land pixels before return (tests / fit-to-extent).
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    vir_viewport1_ = m_Viewport;
    vir_viewport2_ = m_Viewport;
  }
  note_painted_preview_baseline();
  m_curDrawingOrg.x = 0;
  m_curDrawingOrg.y = 0;

  if (bRealTime) {
    sync_host_paint_context();
    layer_painter_.render_map(
        pSmtMap, static_cast<int>(m_Viewport.m_fVOX),
        static_cast<int>(m_Viewport.m_fVOY),
        static_cast<int>(m_Viewport.m_fVWidth),
        static_cast<int>(m_Viewport.m_fVHeight), R2_COPYPEN);
    Refresh();
    return SMT_ERR_NONE;
  }
  return ReRenderMapByProxy(pSmtMap, static_cast<int>(m_Viewport.m_fVOX),
                            static_cast<int>(m_Viewport.m_fVOY),
                            static_cast<int>(m_Viewport.m_fVWidth),
                            static_cast<int>(m_Viewport.m_fVHeight));
}

int SmtGdiRenderDevice::BeginRender(eRDBufferLayer eMRDBufLyr, bool bClear,
                                    const SmtStyle *pStyle, int op) {
  sync_host_paint_context();
  paint_canvas_.lock_style() = (NULL != pStyle);

  switch (eMRDBufLyr) {
    case MRD_BL_MAP: {
      // Shared front is owned by the worker publish path while busy.
      // Default clear matches GdiOwnedSurface::clear ocean key.
      if (!begin_surface_encode_pass(map_front_, RGB(170, 211, 223), bClear,
                                     /*fail_if_busy=*/true)) {
        return SMT_ERR_FAILURE;
      }
    } break;
    case MRD_BL_DYNAMIC: {
      // White: TransparentBlt key in RenderMap/Refresh. Ocean clear would
      // paint an opaque overlay and wipe the map composite.
      if (!begin_surface_encode_pass(dynamic_buf_, RGB(255, 255, 255), bClear,
                                     /*fail_if_busy=*/false)) {
        return SMT_ERR_FAILURE;
      }
    } break;
    case MRD_BL_QUICK: {
      if (!begin_surface_encode_pass(raster_back_, RGB(255, 255, 255), bClear,
                                     /*fail_if_busy=*/false)) {
        return SMT_ERR_FAILURE;
      }
    } break;
    case MRD_BL_DIRECT: {
      // HWND DC path �?no command encoder (cannot replay against a surface).
      if (bClear) {
        RECT rt;
        GetClientRect(m_hWnd, &rt);
        InvalidateRect(m_hWnd, &rt, FALSE);
      }

      paint_canvas_.set_dc(GetDC(m_hWnd));
    } break;
  }

  if (paint_canvas_.lock_style()) PrepareForDrawing(pStyle, op);

  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::EndRender(eRDBufferLayer eMRDBufLyr) {
  if (paint_canvas_.lock_style()) EndDrawing();

  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      end_surface_encode_pass(map_front_);
      break;
    case MRD_BL_DYNAMIC:
      end_surface_encode_pass(dynamic_buf_);
      break;
    case MRD_BL_QUICK:
      end_surface_encode_pass(raster_back_);
      break;
    case MRD_BL_DIRECT:
      ReleaseDC(m_hWnd, paint_canvas_.dc());
      break;
  }

  paint_canvas_.set_dc(NULL);
  paint_canvas_.lock_style() = false;

  return SMT_ERR_NONE;
}

void SmtGdiRenderDevice::sync_host_paint_context() {
  host_rc_.viewport = m_Viewport;
  host_rc_.windowport = m_Windowport;
  host_rc_.fblc = static_cast<float>(m_fblc);
  layer_painter_.set_context(&host_rc_);
  layer_painter_.set_render_pra(&m_rdPra);
  paint_canvas_.set_context(&host_rc_);
  paint_canvas_.set_render_pra(&m_rdPra);
}

int SmtGdiRenderDevice::rerender_map(const SmtMap *map, bool realtime) {
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

bool SmtGdiRenderDevice::begin_surface_encode_pass(GdiOwnedSurface &buf,
                                                   COLORREF clear_color,
                                                   bool clear,
                                                   bool fail_if_busy) {
  if (fail_if_busy && render_thread_ && render_thread_->is_busy()) {
    return false;
  }
  {
    BASE_TRACE_EVENT("begin_pass", "gdi.encode");
    pass_encoder_.begin_pass(&buf.surface());
  }
  paint_canvas_.set_encoder(&pass_encoder_);
  if (clear) {
    pass_encoder_.clear(clear_color);
  }
  // Record-only: Draw* push encoder ops; EndRender replays onto |buf|.
  return true;
}

void SmtGdiRenderDevice::end_surface_encode_pass(GdiOwnedSurface &buf) {
  paint_canvas_.set_encoder(nullptr);
  finish_host_encode_pass(pass_encoder_, last_pass_);
  {
    BASE_TRACE_EVENT("replay", "gdi.encode");
    detail::replay(last_pass_, buf.surface());
  }
}

int SmtGdiRenderDevice::ScheduleDelayedRedraw(const SmtMap *pMap) {
  return ui_controller_.schedule_delayed_redraw(pMap);
}

int SmtGdiRenderDevice::stage_map_job(const SmtMap *pMap, int x, int y, int w,
                                      int h, int op, bool urgent) {
  return ui_controller_.stage_map_job(pMap, x, y, w, h, op, urgent);
}

bool SmtGdiRenderDevice::submit_staged_job() {
  return ui_controller_.submit_staged_job();
}

void SmtGdiRenderDevice::invalidate_map_present() {
  ui_controller_.invalidate_map_present();
}

int SmtGdiRenderDevice::Timer() { return ui_controller_.on_timer(); }

int SmtGdiRenderDevice::PrepareForDrawing(const SmtStyle *pStyle,
                                          int nDrawMode) {
  return paint_canvas_.prepare_for_drawing(pStyle, nDrawMode);
}

int SmtGdiRenderDevice::EndDrawing() { return paint_canvas_.end_drawing(); }

int SmtGdiRenderDevice::RenderMap(void) { return RenderMapToDC(nullptr); }

int SmtGdiRenderDevice::RenderMapToDC(HDC hdc) {
  // Compose under the shared-front lock; present/BitBlt after unlock so HWND
  // message re-entry cannot deadlock with Timer Refresh.
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    detail::clamp_preview_dest(&vir_viewport2_, m_Viewport);

    compose_buf_.clear(m_Viewport.m_fVOX, m_Viewport.m_fVOY,
                       m_Viewport.m_fVWidth, m_Viewport.m_fVHeight);

    map_front_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                       vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                       vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                       vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                       vir_viewport2_.m_fVHeight, BLT_STRETCH, SRCCOPY);

    dynamic_buf_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                         vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                         vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                         vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                         vir_viewport2_.m_fVHeight, BLT_TRANSPARENT, SRCCOPY);

    raster_back_.blit_to(compose_buf_, vir_viewport1_.m_fVOX,
                         vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                         vir_viewport1_.m_fVHeight, vir_viewport2_.m_fVOX,
                         vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                         vir_viewport2_.m_fVHeight, BLT_TRANSPARENT, SRCCOPY);
  }

  if (hdc) {
    // Fill uncovered pan margins (shifted blit) with the map ocean key.
    {
      detail::GdiPlayer player(hdc);
      player.clear_rect(0, 0, static_cast<int>(m_Viewport.m_fVWidth),
                        static_cast<int>(m_Viewport.m_fVHeight));
    }
    HDC hSrcDC = compose_buf_.prepare_dc(false);
    const BOOL ok = ::BitBlt(hdc, m_curDrawingOrg.x, m_curDrawingOrg.y,
                             static_cast<int>(m_Viewport.m_fVWidth),
                             static_cast<int>(m_Viewport.m_fVHeight), hSrcDC,
                             static_cast<int>(m_Viewport.m_fVOX),
                             static_cast<int>(m_Viewport.m_fVOY), SRCCOPY);
    compose_buf_.end_dc();
    return ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

  return compose_buf_.present(m_curDrawingOrg.x, m_curDrawingOrg.y,
                              m_Viewport.m_fVWidth, m_Viewport.m_fVHeight,
                              m_Viewport.m_fVOX, m_Viewport.m_fVOY);
}

int SmtGdiRenderDevice::ReRenderMapByProxy(const SmtMap *pMap, int x, int y,
                                           int w, int h, int op) {
  if (w == 0 || h == 0) return SMT_ERR_INVALID_PARAM;
  if (!render_thread_) return SMT_ERR_FAILURE;

  // MapLibre settle: keep last-good front (no white clear flash). Stage a
  // debounced job; Timer submits after ~200 ms idle. Present preview now.
  const int staged = stage_map_job(pMap, x, y, w, h, op, /*urgent=*/false);
  if (staged != SMT_ERR_NONE) {
    return staged;
  }
  Refresh();
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::ReRenderMapRealTime(const SmtMap *pMap, int x, int y,
                                            int w, int h, int op) {
  if (w == 0 || h == 0) return SMT_ERR_INVALID_PARAM;
  if (!render_thread_) return SMT_ERR_FAILURE;

  // Interactive: urgent FrameJob, never Sleep-poll on the UI thread.
  // stage_map_job cancels in-flight when busy (no sticky idle cancel).
  const int staged = stage_map_job(pMap, x, y, w, h, op, /*urgent=*/true);
  if (staged != SMT_ERR_NONE) {
    return staged;
  }
  if (submit_staged_job()) {
    Refresh();
    return SMT_ERR_NONE;
  }
  if (render_thread_->is_busy()) {
    Refresh();
    return SMT_ERR_NONE;
  }

  // Worker idle but submit missed (executor race): sync paint once.
  sync_host_paint_context();
  layer_painter_.render_map(pMap, x, y, w, h, op);
  note_painted_preview_baseline();
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    vir_viewport1_ = m_Viewport;
    vir_viewport2_ = m_Viewport;
  }
  Refresh();
  return SMT_ERR_NONE;
}

int SmtGdiRenderDevice::RenderMap(const SmtMap *pMap, int op) {
  sync_host_paint_context();
  return layer_painter_.render_map(pMap, static_cast<int>(m_Viewport.m_fVOX),
                                   static_cast<int>(m_Viewport.m_fVOY),
                                   static_cast<int>(m_Viewport.m_fVWidth),
                                   static_cast<int>(m_Viewport.m_fVHeight), op);
}

int SmtGdiRenderDevice::RenderLayer(const SmtLayer *pLayer, int op) {
  sync_host_paint_context();
  return layer_painter_.render_layer(pLayer, op);
}

int SmtGdiRenderDevice::RenderLayer(OGRLayer *pLayer, int op) {
  sync_host_paint_context();
  return layer_painter_.render_layer(pLayer, op);
}

int SmtGdiRenderDevice::RenderLayer(const SmtRasterLayer *pLayer, int op) {
  sync_host_paint_context();
  return layer_painter_.render_layer(pLayer, op);
}

int SmtGdiRenderDevice::RenderLayer(const SmtTileLayer *pLayer, int op) {
  sync_host_paint_context();
  return layer_painter_.render_layer(pLayer, op);
}

int SmtGdiRenderDevice::RenderFeature(OGRFeature *pFeature, int op) {
  sync_host_paint_context();
  return layer_painter_.render_feature(pFeature, op);
}

int SmtGdiRenderDevice::RenderGeometry(const OGRGeometry *pGeom,
                                       const SmtStyle *pStyle, int op) {
  sync_host_paint_context();
  return layer_painter_.render_geometry(pGeom, pStyle, op);
}

int SmtGdiRenderDevice::DrawMultiLineString(
    const OGRMultiLineString *pMultiLinestring) {
  return paint_canvas_.draw_multi_line_string(pMultiLinestring);
}

int SmtGdiRenderDevice::DrawMultiPoint(const SmtStyle *pStyle,
                                       const OGRMultiPoint *pMultiPoint) {
  return paint_canvas_.draw_multi_point(pStyle, pMultiPoint);
}

int SmtGdiRenderDevice::DrawMultiPolygon(const OGRMultiPolygon *pMultiPolygon) {
  return paint_canvas_.draw_multi_polygon(pMultiPolygon);
}

int SmtGdiRenderDevice::DrawPoint(const SmtStyle *pStyle,
                                  const OGRPoint *pPoint) {
  return paint_canvas_.draw_point(pStyle, pPoint);
}

int SmtGdiRenderDevice::DrawAnno(const char *szAnno, float fangel,
                                 float fCHeight, float fCWidth, float fCSpace,
                                 const OGRPoint *pPoint) {
  return paint_canvas_.draw_anno(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                 pPoint);
}

int SmtGdiRenderDevice::DrawSymbol(HICON hIcon, long lHeight, long lWidth,
                                   const OGRPoint *pPoint) {
  return paint_canvas_.draw_symbol(hIcon, lHeight, lWidth, pPoint);
}

int SmtGdiRenderDevice::DrawLineSpline(const OGRLineString *pSpline) {
  return paint_canvas_.draw_line_spline(pSpline);
}

int SmtGdiRenderDevice::DrawLineString(const OGRLineString *pLinestring) {
  return paint_canvas_.draw_line_string(pLinestring);
}

int SmtGdiRenderDevice::DrawLinearRing(const OGRLinearRing *pLinearRing) {
  return paint_canvas_.draw_linear_ring(pLinearRing);
}

int SmtGdiRenderDevice::DrawPloygon(const OGRPolygon *pPloygon) {
  return paint_canvas_.draw_polygon(pPloygon);
}

int SmtGdiRenderDevice::DrawTin(const SmtTin *pTin) {
  return paint_canvas_.draw_tin(pTin);
}

int SmtGdiRenderDevice::DrawGrid(const SmtGrid *pGrid) {
  return paint_canvas_.draw_grid(pGrid);
}

int SmtGdiRenderDevice::DrawFan(const OGRPolygon *pFan) {
  return paint_canvas_.draw_fan(pFan);
}

int SmtGdiRenderDevice::DrawArc(const OGRLineString *pArc) {
  return paint_canvas_.draw_arc(pArc);
}

int SmtGdiRenderDevice::DrawEllipse(float left, float top, float right,
                                    float bottom, bool bDP) {
  return paint_canvas_.draw_ellipse(left, top, right, bottom, bDP);
}

int SmtGdiRenderDevice::DrawRect(const fRect &rect, bool bDP) {
  return paint_canvas_.draw_rect(rect, bDP);
}

int SmtGdiRenderDevice::DrawLine(fPoint *pfPoints, int nCount, bool bDP) {
  return paint_canvas_.draw_line(pfPoints, nCount, bDP);
}

int SmtGdiRenderDevice::DrawLine(const fPoint &ptA, const fPoint &ptB,
                                 bool bDP) {
  return paint_canvas_.draw_line(ptA, ptB, bDP);
}

int SmtGdiRenderDevice::DrawText(const char *szAnno, float fangel,
                                 float fCHeight, float fCWidth, float fCSpace,
                                 const fPoint &point, bool bDP) {
  return paint_canvas_.draw_text(szAnno, fangel, fCHeight, fCWidth, fCSpace,
                                 point, bDP);
}

int SmtGdiRenderDevice::DrawImage(const char *szImageBuf, int nImageBufSize,
                                  const fRect &frect, long lCodeType,
                                  eRDBufferLayer eMRDBufLyr) {
  return image_io_.draw_image(szImageBuf, nImageBufSize, frect, lCodeType,
                              eMRDBufLyr);
}

int SmtGdiRenderDevice::StrethImage(const char *szImageBuf, int nImageBufSize,
                                    const fRect &frect, long lCodeType,
                                    eRDBufferLayer eMRDBufLyr) {
  return image_io_.streth_image(szImageBuf, nImageBufSize, frect, lCodeType,
                                eMRDBufLyr);
}

int SmtGdiRenderDevice::SaveImage(const char *szFilePath,
                                  eRDBufferLayer eMRDBufLyr,
                                  bool bBgTransparent) {
  return image_io_.save_image(szFilePath, eMRDBufLyr, bBgTransparent);
}

int SmtGdiRenderDevice::Save2ImageBuf(char *&szImageBuf, long &lImageBufSize,
                                      long lCodeType, eRDBufferLayer eMRDBufLyr,
                                      bool bBgTransparent) {
  return image_io_.save2_image_buf(szImageBuf, lImageBufSize, lCodeType,
                                   eMRDBufLyr, bBgTransparent);
}

int SmtGdiRenderDevice::FreeImageBuf(char *&szImageBuf) {
  return image_io_.free_image_buf(szImageBuf);
}

}  // namespace render
