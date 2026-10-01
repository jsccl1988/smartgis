// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_SCHEDULER_H_
#define LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_SCHEDULER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "base/execution/execution_executor.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/context.h"

namespace render {
namespace detail {

// Serial leftover FrameJob lane (Chromium Impl analogue for map2d).
// Coalesces staged context while busy; HWND-safe shutdown (no join).
// Inside paint_fn, tile execute fans out on Rhi2dTileGraphRunner (Raster×N).
class Rhi2dScheduler {
 public:
  using PaintFn = std::function<void()>;

  Rhi2dScheduler() = default;
  ~Rhi2dScheduler() = default;

  Rhi2dScheduler(const Rhi2dScheduler&) = delete;
  Rhi2dScheduler& operator=(const Rhi2dScheduler&) = delete;

  void set_paint_fn(PaintFn fn);

  void start();
  // Bump job_gen, clear cancel, post paint_loop if not scheduled.
  void submit();

  // Coalesce into pending if busy; otherwise apply immediately.
  void stage_context(const SmtRenderContext& rc);
  // Apply only when !is_busy().
  void set_context(const SmtRenderContext& rc);

  SmtRenderContext& context();
  const SmtRenderContext& context() const;

  void cancel();
  bool wait_idle(int timeout_ms);
  // Returns true if the worker was detached (executor leaked until process
  // exit).
  bool shutdown();

  // UI-thread sync china bootstrap (ZoomToRect) must not overlap a FrameJob
  // on the same back_buf_/painter. Nested MFC pumps can submit mid-sync.
  void begin_sync_paint();
  void end_sync_paint();
  bool sync_paint_active() const {
    return sync_paint_depth_.load(std::memory_order_acquire) > 0;
  }

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
  // Dedicated serial GDI FrameJob lane (size 1). Not the process-global pool —
  // concurrent GDI on the shared front HBITMAP is unsafe.
  std::unique_ptr<base::execution::NThreadPoolExecutor> executor_;
  std::atomic<bool> suspend_{true};
  std::atomic<bool> stop_{false};
  // True while a paint_loop task is queued or running on executor_.
  std::atomic<bool> scheduled_{false};
  // Abandon current FrameJob without tearing down the worker.
  std::atomic<bool> cancel_frame_{false};
  // Bumped on each submit(); publish to front only if still current.
  std::atomic<uint64_t> job_gen_{0};
  std::atomic<uint64_t> published_gen_{0};
  // Set when paint_loop leaves; used after HWND-thread leak path to avoid UAF.
  std::atomic<bool> exited_{true};
  // >0 while HWND-thread paint_map_sync holds the GDI back buffer.
  std::atomic<int> sync_paint_depth_{0};

  SmtRenderContext rc_;
  // Coalesced job while paint runs; applied before the next suspend.
  SmtRenderContext pending_rc_;
  std::atomic<bool> has_pending_{false};
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_COMMON_CC_SCHEDULER_H_
