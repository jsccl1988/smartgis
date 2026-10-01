// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/common/frame/scheduler.h"

#include <windows.h>

#include <chrono>

#include "base/trace/event/process_trace.h"

namespace render {
namespace detail {

void Rhi3dFrameScheduler::set_paint_fn(PaintFn fn) { paint_fn_ = std::move(fn); }

void Rhi3dFrameScheduler::start() {
  if (executor_) {
    return;
  }
  stop_.store(false, std::memory_order_release);
  suspend_.store(true, std::memory_order_release);
  scheduled_.store(false, std::memory_order_release);
  exited_.store(true, std::memory_order_release);
  executor_ =
      std::make_unique<base::execution::NThreadPoolExecutor>(/*size=*/1);
}

void Rhi3dFrameScheduler::submit() {
  BASE_TRACE_EVENT("submit", "rhi3d.frame");
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

void Rhi3dFrameScheduler::stage_request(const Rhi3dFrameRequest& req) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (busy_.load(std::memory_order_acquire)) {
    pending_req_ = req;
    has_pending_.store(true, std::memory_order_release);
    cancel_frame_.store(true, std::memory_order_release);
  } else {
    req_ = req;
    has_pending_.store(false, std::memory_order_release);
  }
}

void Rhi3dFrameScheduler::set_request(const Rhi3dFrameRequest& req) {
  if (!is_busy()) {
    req_ = req;
  }
}

Rhi3dFrameRequest& Rhi3dFrameScheduler::request() { return req_; }

const Rhi3dFrameRequest& Rhi3dFrameScheduler::request() const { return req_; }

void Rhi3dFrameScheduler::cancel() {
  BASE_TRACE_EVENT("cancel", "rhi3d.frame");
  cancel_frame_.store(true, std::memory_order_release);
}

bool Rhi3dFrameScheduler::wait_idle(int timeout_ms) {
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

bool Rhi3dFrameScheduler::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    has_pending_.store(false, std::memory_order_release);
    cancel_frame_.store(true, std::memory_order_release);
    stop_.store(true, std::memory_order_release);
    suspend_.store(false, std::memory_order_release);
  }
  if (!executor_) {
    exited_.store(true, std::memory_order_release);
    return false;
  }
  // Never join from the HWND thread while a FrameJob may still touch the
  // device / HWND — same deadlock class as GDI leftover.
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

bool Rhi3dFrameScheduler::is_busy() const {
  return busy_.load(std::memory_order_acquire);
}

bool Rhi3dFrameScheduler::has_pending() const {
  return has_pending_.load(std::memory_order_acquire);
}

bool Rhi3dFrameScheduler::has_exited() const {
  return exited_.load(std::memory_order_acquire);
}

uint64_t Rhi3dFrameScheduler::job_generation() const {
  return job_gen_.load(std::memory_order_acquire);
}

uint64_t Rhi3dFrameScheduler::published_generation() const {
  return published_gen_.load(std::memory_order_acquire);
}

void Rhi3dFrameScheduler::mark_published(uint64_t gen) {
  published_gen_.store(gen, std::memory_order_release);
}

bool Rhi3dFrameScheduler::should_abort(uint64_t paint_job_gen) const {
  return stop_.load(std::memory_order_acquire) ||
         cancel_frame_.load(std::memory_order_acquire) ||
         job_gen_.load(std::memory_order_acquire) != paint_job_gen;
}

void Rhi3dFrameScheduler::enqueue_paint() {
  if (!executor_) {
    scheduled_.store(false, std::memory_order_release);
    return;
  }
  exited_.store(false, std::memory_order_release);
  (void)executor_->execute([this]() { paint_loop(); });
}

void Rhi3dFrameScheduler::paint_loop() {
  while (!stop_.load(std::memory_order_acquire)) {
    BASE_TRACE_EVENT("paint_loop", "rhi3d.frame");
    busy_.store(true, std::memory_order_release);
    cancel_frame_.store(false, std::memory_order_release);
    if (paint_fn_) {
      paint_fn_();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load(std::memory_order_acquire)) {
      busy_.store(false, std::memory_order_release);
      scheduled_.store(false, std::memory_order_release);
      break;
    }
    if (has_pending_.load(std::memory_order_acquire)) {
      req_ = pending_req_;
      has_pending_.store(false, std::memory_order_release);
      cancel_frame_.store(false, std::memory_order_release);
      job_gen_.fetch_add(1, std::memory_order_acq_rel);
      suspend_.store(false, std::memory_order_release);
      continue;
    }
    suspend_.store(true, std::memory_order_release);
    scheduled_.store(false, std::memory_order_release);
    busy_.store(false, std::memory_order_release);
    break;
  }
  exited_.store(true, std::memory_order_release);
}

}  // namespace detail
}  // namespace render
