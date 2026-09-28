// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_DEVICE_ADAPTER_ID_H_
#define GPU_DEVICE_ADAPTER_ID_H_

#include <cstdint>

namespace gpu {
namespace detail {

// Identifies one GPU adapter / device slot inside the single GPU process.
using AdapterId = uint32_t;

inline constexpr AdapterId kAdapterPrimary = 0;
inline constexpr AdapterId kAdapterInvalid = 0xffffffffu;

// DXGI-facing adapter metadata. No FlyCube / RHI types here.
struct AdapterInfo {
  AdapterId id = kAdapterInvalid;
  bool is_hardware = false;
  uint64_t luid = 0;
  char description[128] = {};
};

}  // namespace detail
}  // namespace gpu

#endif  // GPU_DEVICE_ADAPTER_ID_H_
