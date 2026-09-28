// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_COLUMN_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_COLUMN_H

#include <cstdint>
#include <vector>

namespace base {
namespace execution {

// Host-backed i64 column; device/DLPack stay null until a CUDA backend lands.
class DeviceColumn {
 public:
  static DeviceColumn from_host_i64(const std::int64_t* data, std::size_t n) {
    DeviceColumn c;
    if (data != nullptr && n > 0) {
      c.host_.assign(data, data + n);
    }
    c.from_host_count_ = 1;
    return c;
  }

  std::size_t size() const { return host_.size(); }
  const std::int64_t* host_i64() const {
    return host_.empty() ? nullptr : host_.data();
  }
  std::uint32_t to_host_count() const { return to_host_count_; }
  std::uint32_t from_host_count() const { return from_host_count_; }
  void* dlpack_ptr() const { return nullptr; }

 private:
  std::vector<std::int64_t> host_;
  std::uint32_t from_host_count_ = 0;
  std::uint32_t to_host_count_ = 0;
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_COLUMN_H
