// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// LayerTreeHost drives leftover rhi2d map paint (gis::Map*, not SmtMap).

#include "legacy/render/rhi2d/impl/common/cc/layer_tree_host.h"

#include "base/trace/event/process_trace.h"
#include "legacy/core/macros/macros.h"
#include "legacy/render/detail/frame_pipeline.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/carto_draw.h"
#include "legacy/render/rhi2d/impl/common/paint/map/map_painter.h"

namespace render {
namespace detail {

Rhi2dLayerTreeHost::Rhi2dLayerTreeHost(HINSTANCE h_inst,
                                       Viewport& vir_viewport1,
                                       Viewport& vir_viewport2,
                                       std::mutex* shared_front_mu)
    : h_inst_(h_inst),
      vir_viewport1_(vir_viewport1),
      vir_viewport2_(vir_viewport2),
      carto_draw_(std::make_unique<Rhi2dCartoDraw>(h_inst)),
      painter_(std::make_unique<Rhi2dPainter>(
          carto_draw_.get(), &back_buf_, &shared_buf_, &vir_viewport1_,
          &vir_viewport2_, shared_front_mu)) {
  bind_painter();
  scheduler_.set_paint_fn([this]() { on_worker_tick(); });
}

Rhi2dLayerTreeHost::~Rhi2dLayerTreeHost() { shutdown(); }

void Rhi2dLayerTreeHost::bind_painter() {
  painter_->set_scheduler(&scheduler_);
  painter_->set_layer_tree(&tree_);
  painter_->set_context(&context());
}

int Rhi2dLayerTreeHost::init(HWND hwnd, const char* logname) {
  if (hwnd == nullptr || logname == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  hwnd_ = hwnd;
  back_buf_.set_wnd(hwnd_);
  (void)logname;
  scheduler_.start();
  return SMT_ERR_NONE;
}

int Rhi2dLayerTreeHost::resize(int orgx, int orgy, int cx, int cy,
                               Rhi2dOwnedSurface& front) {
  if (cx < 0 || cy < 0) {
    return SMT_ERR_FAILURE;
  }
  if (scheduler_.is_busy()) {
    return SMT_ERR_FAILURE;
  }

  SmtRenderContext& rc = context();
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

  if (SMT_ERR_NONE ==
          back_buf_.set_size(static_cast<int>(rc.viewport.m_fVWidth),
                             static_cast<int>(rc.viewport.m_fVHeight)) &&
      SMT_ERR_NONE == shared_buf_.share_from(front)) {
    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}

void Rhi2dLayerTreeHost::commit(const SmtRenderContext& rc) {
  BASE_TRACE_EVENT("commit", "gdi.cc");
  pending_full_damage_ = true;
  pending_damage_ = RECT{0, 0, 0, 0};
  scheduler_.stage_context(rc);
}

void Rhi2dLayerTreeHost::commit(const SmtRenderContext& rc, const RECT& damage,
                                bool full_damage) {
  BASE_TRACE_EVENT("commit_damage", "gdi.cc");
  pending_full_damage_ = full_damage;
  pending_damage_ = damage;
  scheduler_.stage_context(rc);
}

int Rhi2dLayerTreeHost::stage_frame(const SmtRenderContext& ctx,
                                    const Smt2DRenderOptions& options) {
  rd_options_ = options;
  painter_->set_render_options(&rd_options_);
  commit(ctx);
  return SMT_ERR_NONE;
}

void Rhi2dLayerTreeHost::schedule() {
  BASE_TRACE_EVENT("schedule", "gdi.cc");
  scheduler_.submit();
}

bool Rhi2dLayerTreeHost::submit_frame(const SmtRenderContext& ctx,
                                      const Smt2DRenderOptions& options) {
  if (scheduler_.is_busy()) {
    return false;
  }
  rd_options_ = options;
  painter_->set_render_options(&rd_options_);
  scheduler_.set_context(ctx);
  bind_painter();
  schedule();
  return true;
}

int Rhi2dLayerTreeHost::paint_map_sync(const SmtRenderContext& ctx,
                                       const Smt2DRenderOptions& options) {
  // Block nested Timer→submit while draining, then take back_buf_ on HWND.
  scheduler_.begin_sync_paint();
  cancel();
  (void)wait_idle(2000);
  if (is_busy()) {
    scheduler_.end_sync_paint();
    return SMT_ERR_FAILURE;
  }
  rd_options_ = options;
  scheduler_.set_context(ctx);
  // Sync publish: no job gen / abort gate (scheduler + tree unbound).
  painter_->set_scheduler(nullptr);
  painter_->set_layer_tree(nullptr);
  painter_->set_context(&context());
  painter_->set_render_options(&rd_options_);
  const int err =
      painter_->render_map(ctx.pMap, ctx.orgx, ctx.orgy, ctx.width, ctx.height,
                           ctx.op);
  bind_painter();
  scheduler_.end_sync_paint();
  return err;
}

void Rhi2dLayerTreeHost::cancel() { scheduler_.cancel(); }

bool Rhi2dLayerTreeHost::wait_idle(int timeout_ms) {
  return scheduler_.wait_idle(timeout_ms);
}

bool Rhi2dLayerTreeHost::shutdown() { return scheduler_.shutdown(); }

bool Rhi2dLayerTreeHost::is_busy() const { return scheduler_.is_busy(); }

bool Rhi2dLayerTreeHost::has_pending() const {
  return scheduler_.has_pending();
}

bool Rhi2dLayerTreeHost::has_exited() const { return scheduler_.has_exited(); }

uint64_t Rhi2dLayerTreeHost::job_generation() const {
  return scheduler_.job_generation();
}

uint64_t Rhi2dLayerTreeHost::published_generation() const {
  return scheduler_.published_generation();
}

void Rhi2dLayerTreeHost::mark_published(uint64_t gen) {
  scheduler_.mark_published(gen);
}

bool Rhi2dLayerTreeHost::should_abort(uint64_t paint_job_gen) const {
  return scheduler_.should_abort(paint_job_gen);
}

bool Rhi2dLayerTreeHost::can_publish(uint64_t paint_job_gen) const {
  return tree_.is_current(paint_job_gen) &&
         !scheduler_.should_abort(paint_job_gen);
}

SmtRenderContext& Rhi2dLayerTreeHost::context() { return scheduler_.context(); }

const SmtRenderContext& Rhi2dLayerTreeHost::context() const {
  return scheduler_.context();
}

void Rhi2dLayerTreeHost::apply_pending_damage() {
  tree_.set_damage(pending_damage_, pending_full_damage_);
}

void Rhi2dLayerTreeHost::on_worker_tick() {
  BASE_TRACE_EVENT("activate_draw", "gdi.cc");
  const uint64_t gen = scheduler_.job_generation();
  if (scheduler_.should_abort(gen)) {
    tree_.retire();
    return;
  }
  apply_pending_damage();
  (void)tree_.activate(gen, scheduler_.context());
  paint_once();
  if (scheduler_.should_abort(gen)) {
    tree_.retire();
  }
}

void Rhi2dLayerTreeHost::paint_once() {
  BASE_TRACE_EVENT("RenderMap", "gdi.frame");
  log_legacy_flow("gdi.RenderMap begin");
  const uint64_t gen = scheduler_.job_generation();
  if (!tree_.is_current(gen) || scheduler_.should_abort(gen)) {
    log_legacy_flow("gdi.RenderMap retire");
    return;
  }
  bind_painter();
  painter_->set_render_options(&rd_options_);
  const SmtRenderContext& rc = context();
  painter_->render_map(rc.pMap, rc.orgx, rc.orgy, rc.width, rc.height, rc.op);
  finish_legacy_frame_memory_sample();
  log_legacy_flow("gdi.RenderMap end");
}

}  // namespace detail
}  // namespace render
