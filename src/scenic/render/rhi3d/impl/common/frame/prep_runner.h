// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI3D_IMPL_COMMON_FRAME_PREP_RUNNER_H_
#define SCENIC_RHI3D_IMPL_COMMON_FRAME_PREP_RUNNER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace scenic {
namespace detail {

// Default resident worker count for leftover 3D CPU prep.
// SMT_RHI3D_PREP_PARALLEL=0 → 1; else clamp(2, 4, hardware_concurrency/2).
int rhi3d_prep_worker_count();

// Resident workers that pull prep job indices (TaskGraphRunner-slim).
// No PostTask-per-job storm — threads stay alive between frames.
// Call only from the leftover FrameJob / sync present thread; workers must
// not touch GL/D3D device APIs.
class Rhi3dPrepRunner {
 public:
  using JobFn = std::function<void(size_t job_index)>;

  Rhi3dPrepRunner() = default;
  ~Rhi3dPrepRunner();

  Rhi3dPrepRunner(const Rhi3dPrepRunner&) = delete;
  Rhi3dPrepRunner& operator=(const Rhi3dPrepRunner&) = delete;

  void ensure_workers(int worker_count);

  // Run |job_count| jobs with |fn|; blocks until all done or cancelled.
  void run_jobs(size_t job_count, JobFn fn);

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

  JobFn fn_;
  size_t job_count_ = 0;
  std::atomic<size_t> next_index_{0};
  std::atomic<size_t> finished_{0};
  bool frame_active_ = false;
};

// Process-wide leftover 3D prep pool (resident workers). Not joined from HWND.
Rhi3dPrepRunner& rhi3d_shared_prep_runner();

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI3D_IMPL_COMMON_FRAME_PREP_RUNNER_H_
