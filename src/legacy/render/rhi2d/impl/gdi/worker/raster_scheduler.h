// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI2D_IMPL_GDI_RASTER_SCHEDULER_H_
#define LEGACY_RENDER_RHI2D_IMPL_GDI_RASTER_SCHEDULER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "base/execution/execution_executor.h"
#include "legacy/render/rhi2d/impl/gdi/paint/canvas/paint_context.h"

namespace render {
namespace detail {

// Serial GDI FrameJob lane: start/suspend/submit, coalesce staged context while
// busy, cancel, wait_idle, and HWND-safe shutdown (no join / leak executor).
class GdiRasterScheduler {
 public:
  // Called once per frame inside paint_loop while the busy flag is set.
  using PaintFn = std::function<void()>;

  GdiRasterScheduler() = default;
  ~GdiRasterScheduler() = default;

  GdiRasterScheduler(const GdiRasterScheduler&) = delete;
  GdiRasterScheduler& operator=(const GdiRasterScheduler&) = delete;

  void set_paint_fn(PaintFn fn);

  void start();
  void suspend();
  // Old resume: bump job_gen, clear cancel, post paint_loop if not scheduled.
  void submit();

  // Coalesce into pending if busy; otherwise apply immediately.
  void stage_context(const SmtRenderContex& rc);
  // Apply only when !is_busy().
  void set_context(const SmtRenderContex& rc);

  SmtRenderContex& context();
  const SmtRenderContex& context() const;

  void cancel();
  bool wait_idle(int timeout_ms);
  // Returns true if the worker was detached (executor leaked until process
  // exit).
  bool shutdown();

  bool is_busy() const;
  bool is_scheduled() const;
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

  // Null pMap on shutdown so a late paint cannot UAF freed layers.
  void clear_map_alias();

 private:
  // Post one FrameJob onto the dedicated 1-thread execution pool.
  void enqueue_paint();
  // Runs on the execution worker: paint + coalesce pending jobs.
  void paint_loop();

  PaintFn paint_fn_;

  std::atomic<bool> busy_{false};
  std::mutex mutex_;
  // Dedicated serial GDI FrameJob lane (size 1). Not the process-global pool -
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

  SmtRenderContex rc_;
  // Coalesced job while paint runs; applied before the next suspend.
  SmtRenderContex pending_rc_;
  std::atomic<bool> has_pending_{false};
};

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_RHI2D_IMPL_GDI_RASTER_SCHEDULER_H_
