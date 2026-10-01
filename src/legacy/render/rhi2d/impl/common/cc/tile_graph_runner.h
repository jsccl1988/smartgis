// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_TILE_GRAPH_RUNNER_H_
#define LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_TILE_GRAPH_RUNNER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace render {
namespace detail {

// Resident raster workers that pull tile indices (TaskGraphRunner-slim).
// Not NThreadPoolExecutor PostTask-per-tile — threads stay alive and wait
// on a condition variable between frames.
class Rhi2dTileGraphRunner {
 public:
  using TileFn = std::function<void(size_t tile_index)>;

  Rhi2dTileGraphRunner() = default;
  ~Rhi2dTileGraphRunner();

  Rhi2dTileGraphRunner(const Rhi2dTileGraphRunner&) = delete;
  Rhi2dTileGraphRunner& operator=(const Rhi2dTileGraphRunner&) = delete;

  // Start or resize resident workers to |worker_count| (>=1).
  void ensure_workers(int worker_count);

  // Run |tile_count| jobs with |fn|; blocks until all done or cancelled.
  // Safe to call from the leftover FrameJob (Impl) thread.
  void run_tiles(size_t tile_count, TileFn fn);

  void request_cancel();
  void clear_cancel();
  bool is_cancel_requested() const;

  void shutdown();

  int worker_count() const { return static_cast<int>(workers_.size()); }

 private:
  void worker_main(int worker_index);
  void join_workers();

  std::mutex mu_;
  std::condition_variable cv_work_;
  std::condition_variable cv_done_;

  std::vector<std::thread> workers_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> cancel_{false};

  TileFn fn_;
  size_t tile_count_ = 0;
  std::atomic<size_t> next_index_{0};
  std::atomic<size_t> finished_{0};
  bool frame_active_ = false;
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_TILE_GRAPH_RUNNER_H_
