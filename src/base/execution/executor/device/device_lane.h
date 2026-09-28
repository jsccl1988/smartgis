// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_LANE_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_LANE_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "base/execution/executor/device/gpu_executor.h"

namespace base {
namespace execution {

enum class lane_qos : std::uint8_t { kHigh = 0, kNormal = 1, kLow = 2 };

// Schedulable device band. CUDA streams come from GPUExecutor; otherwise host.
class DeviceLane {
 public:
  explicit DeviceLane(int device_id = 0, std::size_t stream_count = 4,
                      lane_qos qos = lane_qos::kNormal)
      : exec_(device_id, stream_count), qos_(qos) {}

  int device_id() const { return exec_.device_id(); }
  lane_qos qos() const { return qos_; }
  bool is_gpu_enabled() const { return exec_.is_gpu_enabled(); }

  void* host_alloc(std::size_t bytes) { return std::malloc(bytes); }
  void host_free(void* p) { std::free(p); }

  template <typename Fn, typename... Args>
  auto enqueue(Fn&& fn, Args&&... args) {
    return exec_.execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }

  void synchronize() {
#if HAVE_CUDA
    exec_.synchronize_streams();
#endif
  }

 private:
  GPUExecutor exec_;
  lane_qos qos_;
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_LANE_H
