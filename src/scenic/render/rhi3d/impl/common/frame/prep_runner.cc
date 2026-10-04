// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/common/frame/prep_runner.h"

#include <algorithm>
#include <cstdlib>
#include <thread>

#include "base/trace/event/process_trace.h"

namespace scenic {
namespace detail {

int rhi3d_prep_worker_count() {
  const char* e = std::getenv("SMT_RHI3D_PREP_PARALLEL");
  if (e && e[0] &&
      (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
       e[0] == 'F')) {
    return 1;
  }
  const unsigned hc = std::thread::hardware_concurrency();
  if (hc <= 1) {
    return 1;
  }
  int n = static_cast<int>(hc / 2);
  if (n < 2) {
    n = 2;
  }
  if (n > 4) {
    n = 4;
  }
  return n;
}

Rhi3dPrepRunner& rhi3d_shared_prep_runner() {
  // Intentionally leaked until process exit — HWND must never join workers.
  static Rhi3dPrepRunner* runner = new Rhi3dPrepRunner();
  return *runner;
}

Rhi3dPrepRunner::~Rhi3dPrepRunner() { shutdown(); }

void Rhi3dPrepRunner::ensure_workers(int worker_count) {
  if (worker_count < 1) {
    worker_count = 1;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (static_cast<int>(workers_.size()) == worker_count &&
      !stop_.load(std::memory_order_acquire)) {
    return;
  }
  stop_.store(true, std::memory_order_release);
  cv_work_.notify_all();
  for (std::thread& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();
  stop_.store(false, std::memory_order_release);
  cancel_.store(false, std::memory_order_release);
  workers_.reserve(static_cast<size_t>(worker_count));
  for (int i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this, i]() { worker_main(i); });
  }
}

void Rhi3dPrepRunner::worker_main(int /*worker_index*/) {
  for (;;) {
    JobFn local_fn;
    size_t index = 0;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_work_.wait(lock, [this]() {
        return stop_.load(std::memory_order_acquire) ||
               (frame_active_ &&
                next_index_.load(std::memory_order_acquire) < job_count_);
      });
      if (stop_.load(std::memory_order_acquire) && !frame_active_) {
        return;
      }
      if (!frame_active_) {
        continue;
      }
      index = next_index_.fetch_add(1, std::memory_order_acq_rel);
      if (index >= job_count_) {
        continue;
      }
      local_fn = fn_;
    }

    if (local_fn && !cancel_.load(std::memory_order_acquire)) {
      local_fn(index);
    }

    const size_t done = finished_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (done >= job_count_) {
      std::lock_guard<std::mutex> lock(mu_);
      frame_active_ = false;
      cv_done_.notify_all();
    }
  }
}

void Rhi3dPrepRunner::run_jobs(size_t job_count, JobFn fn) {
  BASE_TRACE_EVENT("run_jobs", "rhi3d.prep");
  if (job_count == 0) {
    return;
  }
  if (workers_.empty()) {
    ensure_workers(rhi3d_prep_worker_count());
  }

  clear_cancel();
  {
    std::lock_guard<std::mutex> lock(mu_);
    fn_ = std::move(fn);
    job_count_ = job_count;
    next_index_.store(0, std::memory_order_release);
    finished_.store(0, std::memory_order_release);
    frame_active_ = true;
  }
  cv_work_.notify_all();

  std::unique_lock<std::mutex> lock(mu_);
  cv_done_.wait(lock, [this]() { return !frame_active_; });
  fn_ = nullptr;
  job_count_ = 0;
}

void Rhi3dPrepRunner::request_cancel() {
  cancel_.store(true, std::memory_order_release);
}

void Rhi3dPrepRunner::clear_cancel() {
  cancel_.store(false, std::memory_order_release);
}

bool Rhi3dPrepRunner::is_cancel_requested() const {
  return cancel_.load(std::memory_order_acquire);
}

void Rhi3dPrepRunner::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_.store(true, std::memory_order_release);
    frame_active_ = false;
    fn_ = nullptr;
    job_count_ = 0;
  }
  cv_work_.notify_all();
  cv_done_.notify_all();
  join_workers();
}

void Rhi3dPrepRunner::join_workers() {
  for (std::thread& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();
}

}  // namespace detail
}  // namespace scenic
