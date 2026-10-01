// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/cc/scheduler.h"

#include <windows.h>

#include <chrono>

#include "base/trace/event/process_trace.h"

namespace render {
namespace detail {

void Rhi2dScheduler::set_paint_fn(PaintFn fn) { paint_fn_ = std::move(fn); }

void Rhi2dScheduler::start() {
  if (executor_) {
    return;
  }
  stop_.store(false, std::memory_order_release);
  suspend_.store(true, std::memory_order_release);
  scheduled_.store(false, std::memory_order_release);
  exited_.store(true, std::memory_order_release);
  // One serial lane: GDI FrameJobs must not overlap on the shared front.
  executor_ =
      std::make_unique<base::execution::NThreadPoolExecutor>(/*size=*/1);
}

void Rhi2dScheduler::submit() {
  BASE_TRACE_EVENT("submit", "gdi.frame");
  if (sync_paint_depth_.load(std::memory_order_acquire) > 0) {
    // Coalesce: UI sync paint owns the buffers; land after end_sync_paint.
    return;
  }
  bool post = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load(std::memory_order_acquire)) {
      return;
    }
    cancel_frame_.store(false, std::memory_order_release);
    job_gen_.fetch_add(1, std::memory_order_acq_rel);
    suspend_.store(false, std::memory_order_release);
    if (!scheduled_.load(std::memory_order_acquire)) {
      scheduled_.store(true, std::memory_order_release);
      post = true;
    }
  }
  if (post) {
    enqueue_paint();
  }
}

void Rhi2dScheduler::begin_sync_paint() {
  sync_paint_depth_.fetch_add(1, std::memory_order_acq_rel);
  cancel_frame_.store(true, std::memory_order_release);
}

void Rhi2dScheduler::end_sync_paint() {
  const int d = sync_paint_depth_.fetch_sub(1, std::memory_order_acq_rel);
  if (d <= 1) {
    cancel_frame_.store(false, std::memory_order_release);
  }
}

void Rhi2dScheduler::stage_context(const SmtRenderContext& rc) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (busy_.load(std::memory_order_acquire)) {
    // Coalesce: abandon the in-flight frame and take the latest context next.
    pending_rc_ = rc;
    has_pending_.store(true, std::memory_order_release);
    cancel_frame_.store(true, std::memory_order_release);
  } else {
    rc_ = rc;
    has_pending_.store(false, std::memory_order_release);
  }
}

void Rhi2dScheduler::set_context(const SmtRenderContext& rc) {
  if (!is_busy()) {
    rc_ = rc;
  }
}

SmtRenderContext& Rhi2dScheduler::context() { return rc_; }

const SmtRenderContext& Rhi2dScheduler::context() const { return rc_; }

void Rhi2dScheduler::cancel() {
  BASE_TRACE_EVENT("cancel", "gdi.frame");
  cancel_frame_.store(true, std::memory_order_release);
}

bool Rhi2dScheduler::wait_idle(int timeout_ms) {
  if (timeout_ms < 0) {
    timeout_ms = 0;
  }
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (busy_.load(std::memory_order_acquire) ||
         scheduled_.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    ::Sleep(1);
  }
  return true;
}

bool Rhi2dScheduler::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Drop map alias before unwind so a late paint cannot UAF layers freed
    // after DestroyWindow returns.
    rc_.pMap = nullptr;
    has_pending_.store(false, std::memory_order_release);
    cancel_frame_.store(true, std::memory_order_release);
    stop_.store(true, std::memory_order_release);
    suspend_.store(false, std::memory_order_release);
  }
  if (!executor_) {
    exited_.store(true, std::memory_order_release);
    return false;
  }
  // Never join the pool from the HWND thread while a FrameJob may still be
  // inside GDI against that HWND — same deadlock as joining std::thread.
  // Wait briefly; if still live, release the executor (leak until process
  // exit).
  (void)wait_idle(200);
  for (int i = 0; i < 200 && !exited_.load(std::memory_order_acquire); ++i) {
    ::Sleep(1);
  }
  if (!exited_.load(std::memory_order_acquire) ||
      scheduled_.load(std::memory_order_acquire)) {
    (void)executor_.release();
    return true;
  }
  executor_.reset();
  return false;
}

bool Rhi2dScheduler::is_busy() const {
  return busy_.load(std::memory_order_acquire);
}

bool Rhi2dScheduler::has_pending() const {
  return has_pending_.load(std::memory_order_acquire);
}

bool Rhi2dScheduler::has_exited() const {
  return exited_.load(std::memory_order_acquire);
}

uint64_t Rhi2dScheduler::job_generation() const {
  return job_gen_.load(std::memory_order_acquire);
}

uint64_t Rhi2dScheduler::published_generation() const {
  return published_gen_.load(std::memory_order_acquire);
}

void Rhi2dScheduler::mark_published(uint64_t gen) {
  published_gen_.store(gen, std::memory_order_release);
}

bool Rhi2dScheduler::should_abort(uint64_t paint_job_gen) const {
  return stop_.load(std::memory_order_acquire) ||
         cancel_frame_.load(std::memory_order_acquire) ||
         job_gen_.load(std::memory_order_acquire) != paint_job_gen;
}

void Rhi2dScheduler::enqueue_paint() {
  if (!executor_) {
    scheduled_.store(false, std::memory_order_release);
    return;
  }
  exited_.store(false, std::memory_order_release);
  (void)executor_->execute([this]() { paint_loop(); });
}

void Rhi2dScheduler::paint_loop() {
  while (!stop_.load(std::memory_order_acquire)) {
    BASE_TRACE_EVENT("paint_loop", "gdi.frame");
    busy_.store(true, std::memory_order_release);
    // Drop sticky cancel left by wait_idle() after the worker went idle.
    // Mid-frame cancel is re-armed by stage_context/cancel while this job runs.
    cancel_frame_.store(false, std::memory_order_release);
    if (paint_fn_) {
      paint_fn_();
    }

    // Keep busy_ set across pending coalesces. Clearing it between frames
    // let UI PreviewZoomScale/Refresh race the next paint on the shared
    // front (wheel zoom AV / exit 1).
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load(std::memory_order_acquire)) {
      busy_.store(false, std::memory_order_release);
      scheduled_.store(false, std::memory_order_release);
      break;
    }
    if (has_pending_.load(std::memory_order_acquire)) {
      // Coalesce into this same execution task (still scheduled_ / busy_).
      rc_ = pending_rc_;
      has_pending_.store(false, std::memory_order_release);
      cancel_frame_.store(false, std::memory_order_release);
      job_gen_.fetch_add(1, std::memory_order_acq_rel);
      suspend_.store(false, std::memory_order_release);
      continue;
    }
    // Drop scheduled under the same lock as suspend so submit() either
    // sees us still scheduled (coalesce) or posts a fresh FrameJob.
    suspend_.store(true, std::memory_order_release);
    scheduled_.store(false, std::memory_order_release);
    busy_.store(false, std::memory_order_release);
    break;
  }
  exited_.store(true, std::memory_order_release);
}

}  // namespace detail
}  // namespace render
