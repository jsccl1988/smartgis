// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_DISAGG_TENSOR_DESC_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_DISAGG_TENSOR_DESC_H

#include <cstdint>

namespace base {
namespace execution {

// Wire-friendly tensor placement for k_disagg nodes. POD only — no RDMA.
struct DisaggTensorDesc {
  int device_id = -1;
  std::uint64_t bytes = 0;
  std::uint32_t rank = 0;
  std::uint64_t shape[8] = {};
  std::uint32_t dtype = 0;  // 0=i64, 1=f32, 2=f64
  std::uint64_t epoch = 0;
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_DISAGG_TENSOR_DESC_H
