// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/gdi/worker/render_thread.h"

#include <cstdio>

#include "base/trace/event/process_trace.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/detail/frame_pipeline.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/layer_painter.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_canvas.h"
#include "legacy/render/rhi2d/impl/gdi/worker/raster_scheduler.h"

namespace render {

SmtGdiRenderThread::SmtGdiRenderThread(HINSTANCE h_inst,
                                       Viewport& vir_viewport1,
                                       Viewport& vir_viewport2,
                                       std::mutex* shared_front_mu)
    : h_inst_(h_inst),
      vir_viewport1_(vir_viewport1),
      vir_viewport2_(vir_viewport2),
      scheduler_(std::make_unique<detail::GdiRasterScheduler>()),
      canvas_(std::make_unique<detail::GdiPaintCanvas>(h_inst)),
      painter_(std::make_unique<detail::GdiLayerPainter>(
          canvas_.get(), &back_buf_, &shared_buf_, &vir_viewport1_,
          &vir_viewport2_, shared_front_mu)) {
  painter_->set_scheduler(scheduler_.get());
  painter_->set_context(&scheduler_->context());
  scheduler_->set_paint_fn([this]() { paint_once(); });
}

SmtGdiRenderThread::~SmtGdiRenderThread() { shutdown(); }

int SmtGdiRenderThread::init(HWND hwnd, const char* logname) {
  if (hwnd == nullptr || logname == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  hwnd_ = hwnd;
  back_buf_.set_wnd(hwnd_);
  (void)logname;
  scheduler_->start();
  return SMT_ERR_NONE;
}

int SmtGdiRenderThread::resize(int orgx, int orgy, int cx, int cy,
                               GdiOwnedSurface& front) {
  if (cx < 0 || cy < 0) {
    return SMT_ERR_FAILURE;
  }
  if (scheduler_->is_busy()) {
    return SMT_ERR_FAILURE;
  }

  SmtRenderContex& rc = scheduler_->context();
  if (is_equal(rc.viewport.m_fVOX, orgx, dEPSILON) &&
      is_equal(rc.viewport.m_fVOY, orgy, dEPSILON) &&
      is_equal(rc.viewport.m_fVHeight, cy, dEPSILON) &&
      is_equal(rc.viewport.m_fVWidth, cx, dEPSILON)) {
    return SMT_ERR_FAILURE;
  }

  rc.viewport.m_fVOX = orgx;
  rc.viewport.m_fVOY = orgy;
  rc.viewport.m_fVHeight = cy;
  rc.viewport.m_fVWidth = cx;

  if (is_equal(rc.windowport.m_fWWidth, 0, dEPSILON) ||
      is_equal(rc.windowport.m_fWHeight, 0, dEPSILON)) {
    rc.fblc = 1.f;
  } else {
    const float xblc = rc.viewport.m_fVWidth / rc.windowport.m_fWWidth;
    const float yblc = rc.viewport.m_fVHeight / rc.windowport.m_fWHeight;
    rc.fblc = (xblc > yblc) ? yblc : xblc;
  }

  // Allocate the private back buffer and alias the device front. Do not
  // HWND-present here: GetDC/BitBlt under SMT_THREAD_SAFE re-enters and
  // deadlocks share_from (gdi_map_paint_test hung in Resize). Host Refresh /
  // Timer owns the first present �?same rule as SmtGdiRenderDevice::Resize.
  if (SMT_ERR_NONE ==
          back_buf_.set_size(static_cast<int>(rc.viewport.m_fVWidth),
                             static_cast<int>(rc.viewport.m_fVHeight)) &&
      SMT_ERR_NONE == shared_buf_.share_from(front)) {
    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}

int SmtGdiRenderThread::stage_frame(const SmtRenderContex& ctx,
                                    const Smt2DRenderPra& pra) {
  rd_pra_ = pra;
  painter_->set_render_pra(&rd_pra_);
  scheduler_->stage_context(ctx);
  return SMT_ERR_NONE;
}

bool SmtGdiRenderThread::submit_frame(const SmtRenderContex& ctx,
                                      const Smt2DRenderPra& pra) {
  if (scheduler_->is_busy()) {
    return false;
  }
  rd_pra_ = pra;
  painter_->set_render_pra(&rd_pra_);
  scheduler_->set_context(ctx);
  painter_->set_context(&scheduler_->context());
  painter_->set_scheduler(scheduler_.get());
  scheduler_->submit();
  return true;
}

void SmtGdiRenderThread::cancel() { scheduler_->cancel(); }

bool SmtGdiRenderThread::wait_idle(int timeout_ms) {
  return scheduler_->wait_idle(timeout_ms);
}

bool SmtGdiRenderThread::try_present(uint64_t* baseline) {
  if (!baseline) {
    return false;
  }
  const uint64_t published = scheduler_->published_generation();
  if (published == *baseline) {
    return false;
  }
  *baseline = published;
  return true;
}

bool SmtGdiRenderThread::shutdown() { return scheduler_->shutdown(); }

bool SmtGdiRenderThread::is_busy() const { return scheduler_->is_busy(); }

bool SmtGdiRenderThread::has_pending() const {
  return scheduler_->has_pending();
}

bool SmtGdiRenderThread::has_exited() const { return scheduler_->has_exited(); }

uint64_t SmtGdiRenderThread::job_generation() const {
  return scheduler_->job_generation();
}

uint64_t SmtGdiRenderThread::published_generation() const {
  return scheduler_->published_generation();
}

void SmtGdiRenderThread::paint_once() {
  BASE_TRACE_EVENT("RenderMap", "gdi.frame");
  detail::log_legacy_flow("gdi.RenderMap begin");
  // FrameJob paints via layer painter encode?replay (see render_map).
  painter_->set_scheduler(scheduler_.get());
  painter_->set_context(&scheduler_->context());
  painter_->set_render_pra(&rd_pra_);
  const SmtRenderContex& rc = scheduler_->context();
  painter_->render_map(rc.pMap, rc.orgx, rc.orgy, rc.width, rc.height, rc.op);
  detail::finish_legacy_frame_memory_sample();
  detail::log_legacy_flow("gdi.RenderMap end");
}

}  // namespace render
