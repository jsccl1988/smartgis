// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/cc/tile_graph_runner.h"

namespace render {
namespace detail {

Rhi2dTileGraphRunner::~Rhi2dTileGraphRunner() { shutdown(); }

void Rhi2dTileGraphRunner::ensure_workers(int worker_count) {
  if (worker_count < 1) {
    worker_count = 1;
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (static_cast<int>(workers_.size()) == worker_count && !stop_.load()) {
    return;
  }
  // Resize requires quiet workers.
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

void Rhi2dTileGraphRunner::worker_main(int /*worker_index*/) {
  for (;;) {
    TileFn local_fn;
    size_t index = 0;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_work_.wait(lock, [this]() {
        return stop_.load(std::memory_order_acquire) ||
               (frame_active_ &&
                next_index_.load(std::memory_order_acquire) < tile_count_);
      });
      if (stop_.load(std::memory_order_acquire) && !frame_active_) {
        return;
      }
      if (!frame_active_) {
        continue;
      }
      index = next_index_.fetch_add(1, std::memory_order_acq_rel);
      if (index >= tile_count_) {
        continue;
      }
      local_fn = fn_;
    }

    if (local_fn && !cancel_.load(std::memory_order_acquire)) {
      local_fn(index);
    }

    const size_t done = finished_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (done >= tile_count_) {
      std::lock_guard<std::mutex> lock(mu_);
      frame_active_ = false;
      cv_done_.notify_all();
    }
  }
}

void Rhi2dTileGraphRunner::run_tiles(size_t tile_count, TileFn fn) {
  if (tile_count == 0) {
    return;
  }
  if (workers_.empty()) {
    ensure_workers(1);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    fn_ = std::move(fn);
    tile_count_ = tile_count;
    next_index_.store(0, std::memory_order_release);
    finished_.store(0, std::memory_order_release);
    frame_active_ = true;
  }
  cv_work_.notify_all();

  std::unique_lock<std::mutex> lock(mu_);
  cv_done_.wait(lock, [this]() { return !frame_active_; });
  fn_ = nullptr;
  tile_count_ = 0;
}

void Rhi2dTileGraphRunner::request_cancel() {
  cancel_.store(true, std::memory_order_release);
}

void Rhi2dTileGraphRunner::clear_cancel() {
  cancel_.store(false, std::memory_order_release);
}

bool Rhi2dTileGraphRunner::is_cancel_requested() const {
  return cancel_.load(std::memory_order_acquire);
}

void Rhi2dTileGraphRunner::shutdown() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_.store(true, std::memory_order_release);
    frame_active_ = false;
    fn_ = nullptr;
    tile_count_ = 0;
  }
  cv_work_.notify_all();
  cv_done_.notify_all();
  join_workers();
}

void Rhi2dTileGraphRunner::join_workers() {
  for (std::thread& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();
}

}  // namespace detail
}  // namespace render
