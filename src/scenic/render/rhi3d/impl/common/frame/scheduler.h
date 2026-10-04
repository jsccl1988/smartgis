// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_COMMON_FRAME_SCHEDULER_H_
#define SCENIC_RHI3D_IMPL_COMMON_FRAME_SCHEDULER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "base/execution/execution_executor.h"
#include "scenic/render/rhi3d/impl/common/frame/frame_request.h"

namespace scenic {
namespace detail {

// Serial leftover 3D FrameJob lane (Chromium Impl analogue for rhi3d).
// Coalesces staged requests while busy; HWND-safe shutdown (no join).
class Rhi3dFrameScheduler {
 public:
  using PaintFn = std::function<void()>;

  Rhi3dFrameScheduler() = default;
  ~Rhi3dFrameScheduler() = default;

  Rhi3dFrameScheduler(const Rhi3dFrameScheduler&) = delete;
  Rhi3dFrameScheduler& operator=(const Rhi3dFrameScheduler&) = delete;

  void set_paint_fn(PaintFn fn);

  void start();
  // Bump job_gen, clear cancel, post paint_loop if not scheduled.
  void submit();

  // Coalesce into pending if busy; otherwise apply immediately.
  void stage_request(const Rhi3dFrameRequest& req);
  // Apply only when !is_busy().
  void set_request(const Rhi3dFrameRequest& req);

  Rhi3dFrameRequest& request();
  const Rhi3dFrameRequest& request() const;

  void cancel();
  bool wait_idle(int timeout_ms);
  // Returns true if the worker was detached (executor leaked until process
  // exit).
  bool shutdown();

  bool is_busy() const;
  bool has_pending() const;
  bool has_exited() const;

  uint64_t job_generation() const;
  uint64_t published_generation() const;
  void mark_published(uint64_t gen);

  // True when stop, cancel, or job generation no longer matches paint_job_gen.
  bool should_abort(uint64_t paint_job_gen) const;
  bool is_cancel_requested() const {
    return cancel_frame_.load(std::memory_order_acquire);
  }

 private:
  void enqueue_paint();
  void paint_loop();

  PaintFn paint_fn_;

  std::atomic<bool> busy_{false};
  std::mutex mutex_;
  // Dedicated serial 3D FrameJob lane (size 1). Device context affinity stays
  // on this worker; do not share GL/D3D Draw with HWND or prep threads.
  std::unique_ptr<base::execution::NThreadPoolExecutor> executor_;
  std::atomic<bool> suspend_{true};
  std::atomic<bool> stop_{false};
  std::atomic<bool> scheduled_{false};
  std::atomic<bool> cancel_frame_{false};
  std::atomic<uint64_t> job_gen_{0};
  std::atomic<uint64_t> published_gen_{0};
  std::atomic<bool> exited_{true};

  Rhi3dFrameRequest req_;
  Rhi3dFrameRequest pending_req_;
  std::atomic<bool> has_pending_{false};
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_COMMON_FRAME_SCHEDULER_H_
