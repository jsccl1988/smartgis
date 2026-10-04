// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#include "scenic/render/rhi2d/impl/common/host/render_device.h"

#include <mutex>

#include "base/core/log.h"
#include "base/math/affine2.h"
#include "base/trace/event/process_trace.h"
#include "scenic/detail/err.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/preview_xform.h"
#include "scenic/render/rhi2d/impl/common/paint/backend/paint_backend.h"
#include "scenic/render/rhi2d/impl/common/cc/layer_tree_host.h"
#include "scenic/render/rhi2d/impl/common/surface/composer/composer.h"

using namespace gis;
using namespace base;

namespace scenic {
namespace detail {
namespace {

// True when a DYNAMIC pass recorded geometry (not only clear / end).
bool pass_has_drawable_overlay(const Rhi2dCommandBuffer &pass) {
  for (const Rhi2dCommand &cmd : pass.ops()) {
    if (cmd.op != Rhi2dCommandOp::kClear && cmd.op != Rhi2dCommandOp::kEndPass) {
      return true;
    }
  }
  return false;
}

bool viewport_equal(const Viewport &a, const Viewport &b) {
  return is_equal(a.m_fVOX, b.m_fVOX, dEPSILON) &&
         is_equal(a.m_fVOY, b.m_fVOY, dEPSILON) &&
         is_equal(a.m_fVWidth, b.m_fVWidth, dEPSILON) &&
         is_equal(a.m_fVHeight, b.m_fVHeight, dEPSILON);
}

// End the host encoder pass and publish last_pass_; emit gdi.encode spans.
void finish_host_encode_pass(detail::Rhi2dCommandEncoder &encoder,
                             Rhi2dCommandBuffer &last_pass) {
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

}  // namespace

void Rhi2dRenderDevice::sync_host_paint_context() {
  host_rc_.viewport = m_Viewport;
  host_rc_.windowport = m_Windowport;
  host_rc_.fblc = static_cast<float>(m_fblc);
  overlay_painter_->set_context(&host_rc_);
  overlay_painter_->set_render_options(&m_rdOptions);
  carto_draw_->set_context(&host_rc_);
  carto_draw_->set_render_options(&m_rdOptions);
}

bool Rhi2dRenderDevice::begin_surface_encode_pass(Rhi2dOwnedSurface &buf,
                                                   COLORREF clear_color,
                                                   bool clear,
                                                   bool fail_if_busy) {
  if (fail_if_busy && layer_tree_host_ && layer_tree_host_->is_busy()) {
    return false;
  }
  {
    BASE_TRACE_EVENT("begin_pass", "gdi.encode");
    if (!pass_encoder_.begin_pass(&buf.surface())) {
      return false;
    }
  }
  carto_draw_->set_encoder(&pass_encoder_);
  if (clear) {
    pass_encoder_.clear(clear_color);
  }
  // Record-only: Draw* push encoder ops; EndRender replays onto |buf|.
  return true;
}

void Rhi2dRenderDevice::end_surface_encode_pass(Rhi2dOwnedSurface &buf) {
  carto_draw_->set_encoder(nullptr);
  finish_host_encode_pass(pass_encoder_, last_pass_);
  {
    BASE_TRACE_EVENT("execute", "gdi.encode");
    detail::execute(last_pass_, buf.surface());
  }
}

int Rhi2dRenderDevice::BeginRender(eRDBufferLayer eMRDBufLyr, bool bClear,
                                    const Style *pStyle, int op) {
  sync_host_paint_context();
  carto_draw_->lock_style() = (nullptr != pStyle);

  switch (eMRDBufLyr) {
    case MRD_BL_MAP: {
      // Shared front is owned by the worker publish path while busy.
      // Default clear matches Rhi2dOwnedSurface::clear ocean key.
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
      // Same shared-front discipline as MAP: raster_back_ is the host/worker
      // paint target; refuse when the FrameJob owns it.
      if (!begin_surface_encode_pass(raster_back_, RGB(255, 255, 255), bClear,
                                     /*fail_if_busy=*/true)) {
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

      carto_draw_->set_dc(GetDC(m_hWnd));
    } break;
  }

  if (carto_draw_->lock_style()) PrepareForDrawing(pStyle, op);

  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::EndRender(eRDBufferLayer eMRDBufLyr) {
  if (carto_draw_->lock_style()) EndDrawing();

  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      end_surface_encode_pass(map_front_);
      break;
    case MRD_BL_DYNAMIC:
      end_surface_encode_pass(dynamic_buf_);
      dynamic_overlay_live_ = pass_has_drawable_overlay(last_pass_);
      break;
    case MRD_BL_QUICK:
      end_surface_encode_pass(raster_back_);
      break;
    case MRD_BL_DIRECT:
      ReleaseDC(m_hWnd, carto_draw_->dc());
      break;
  }

  carto_draw_->set_dc(nullptr);
  carto_draw_->lock_style() = false;

  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::PrepareForDrawing(const Style *pStyle,
                                          int nDrawMode) {
  return carto_draw_->prepare_for_drawing(pStyle, nDrawMode);
}

int Rhi2dRenderDevice::EndDrawing() { return carto_draw_->end_drawing(); }

int Rhi2dRenderDevice::RenderMap(void) { return RenderMapToDC(nullptr); }

int Rhi2dRenderDevice::RenderMapToDC(HDC hdc) {
  // Compose under the shared-front lock; present/BitBlt after unlock so HWND
  // message re-entry cannot deadlock with Timer Refresh.
  //
  // Always lock briefly. try_lock-skip reused stale compose_buf_ so wheel
  // PreviewZoomScale stretch never appeared (zoom_gate ~0). Hang was
  // wait_idle + sync china encode, not this compose lock.
  {
    std::lock_guard<std::mutex> front_lock(shared_front_mu_);
    detail::clamp_preview_dest(&vir_viewport2_, m_Viewport);

    // Pan-only: 1:1 preview + no DYNAMIC �?BitBlt into compose (HWND present
    // owner) without Stretch/clear. Do not present map_front_ directly �?it
    // is not the HWND surface and left the client ocean-blank.
    //
    // Never compose StretchBlt leftovers with a non-zero drawing org: the two
    // transforms shear rivers/boundaries (map shatter). Prefer org slide on
    // an identity front while pan is live.
    const bool pan_slide =
        (m_curDrawingOrg.x != 0 || m_curDrawingOrg.y != 0);
    const bool identity_preview =
        viewport_equal(vir_viewport1_, m_Viewport) &&
        viewport_equal(vir_viewport2_, m_Viewport);
    const bool identity_pan =
        !dynamic_overlay_live_ && (identity_preview || pan_slide);
    if (identity_pan) {
      if (pan_slide && !identity_preview) {
        vir_viewport1_ = m_Viewport;
        vir_viewport2_ = m_Viewport;
      }
      detail::blit_owned_to(
          map_front_, compose_buf_, static_cast<int>(m_Viewport.m_fVOX),
          static_cast<int>(m_Viewport.m_fVOY),
          static_cast<int>(m_Viewport.m_fVWidth),
          static_cast<int>(m_Viewport.m_fVHeight),
          static_cast<int>(m_Viewport.m_fVOX),
          static_cast<int>(m_Viewport.m_fVOY), SRCCOPY);
    } else {
      compose_buf_.clear(m_Viewport.m_fVOX, m_Viewport.m_fVOY,
                         m_Viewport.m_fVWidth, m_Viewport.m_fVHeight);

      // blit_owned_to(dest..., src...): vir_viewport2 is the MapLibre stretch
      // dest (grows on zoom-in); vir_viewport1 is the published front source.
      detail::blit_owned_to(map_front_, compose_buf_, vir_viewport2_.m_fVOX,
                            vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
                            vir_viewport2_.m_fVHeight, vir_viewport1_.m_fVOX,
                            vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
                            vir_viewport1_.m_fVHeight, Rhi2dBlitMode::kOpaque,
                            SRCCOPY);

      if (dynamic_overlay_live_) {
        detail::blit_owned_to(
            dynamic_buf_, compose_buf_, vir_viewport2_.m_fVOX,
            vir_viewport2_.m_fVOY, vir_viewport2_.m_fVWidth,
            vir_viewport2_.m_fVHeight, vir_viewport1_.m_fVOX,
            vir_viewport1_.m_fVOY, vir_viewport1_.m_fVWidth,
            vir_viewport1_.m_fVHeight, Rhi2dBlitMode::kColorKey, SRCCOPY);
      }
    }
  }

  if (hdc) {
    // Fill uncovered pan margins (shifted blit) with the map ocean key.
    {
      detail::ScopedPaintBackend backend(hdc);
      backend->clear_rect(0, 0, static_cast<int>(m_Viewport.m_fVWidth),
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

  return detail::present_to_hwnd(compose_buf_, m_curDrawingOrg.x,
                                 m_curDrawingOrg.y, m_Viewport.m_fVWidth,
                                 m_Viewport.m_fVHeight, m_Viewport.m_fVOX,
                                 m_Viewport.m_fVOY);
}

int Rhi2dRenderDevice::ReRenderMapByProxy(const Map *pMap, int x, int y,
                                           int w, int h, int op) {
  if (w == 0 || h == 0) return SMT_ERR_INVALID_PARAM;
  if (!layer_tree_host_) return SMT_ERR_FAILURE;

  // MapLibre settle: keep last-good front (no white clear flash). Stage a
  // debounced job; Timer submits after ~200 ms idle. Present preview now.
  const int staged = stage_map_job(pMap, x, y, w, h, op, /*urgent=*/false);
  if (staged != SMT_ERR_NONE) {
    return staged;
  }
  Refresh();
  return SMT_ERR_NONE;
}

int Rhi2dRenderDevice::ReRenderMapRealTime(const Map *pMap, int x, int y,
                                            int w, int h, int op) {
  if (w == 0 || h == 0) return SMT_ERR_INVALID_PARAM;
  if (!layer_tree_host_) return SMT_ERR_FAILURE;

  // Interactive: urgent FrameJob, never Sleep-poll or sync-encode on the UI
  // thread. stage_map_job cancels in-flight when busy; Timer retries submit.
  const int staged = stage_map_job(pMap, x, y, w, h, op, /*urgent=*/true);
  if (staged != SMT_ERR_NONE) {
    return staged;
  }
  (void)submit_staged_job();
  Refresh();
  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace scenic
