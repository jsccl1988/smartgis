// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// GPU executor: CUDA/stdgpu path is not enabled in SmartGIS. Falls back to a
// CPU NThreadPoolExecutor so optional umbrella includes stay portable.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_GPU_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_GPU_EXECUTOR_H

#include <cstddef>
#include <utility>

#include "base/core/macros.h"
#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/executor/pool/nthread_executor.h"

namespace base {
namespace execution {

class GPUExecutor {
 public:
  using ThreadContext = NThreadPoolExecutor::ThreadContext;

  explicit GPUExecutor(int /*device_id*/ = 0, std::size_t stream_count = 4)
      : pool_(stream_count ? stream_count
                           : ThreadContext::Thread::hardware_concurrency()) {}

  explicit GPUExecutor(std::size_t stream_count)
      : GPUExecutor(0, stream_count) {}

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn&& fn, Args&&... args) noexcept {
    return pool_.execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }

 private:
  NThreadPoolExecutor pool_;
  DISALLOW_COPY_AND_ASSIGN(GPUExecutor);
};

using GlobalGPUExecutor = GlobalThreadPoolExecutor<GPUExecutor>;

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_GPU_EXECUTOR_H
