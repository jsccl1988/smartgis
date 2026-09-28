// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_LANE_SET_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_LANE_SET_H

#include <cstdint>
#include <memory>
#include <vector>

#include "base/execution/executor/device/device_lane.h"

namespace base {
namespace execution {

// Owns DeviceLane instances and picks one by device_id + QoS floor.
// High=0 < Normal=1 < Low=2, so min_qos=kHigh matches only kHigh.
class LaneSet {
 public:
  // DeviceLane is not movable (GPUExecutor). Reconstruct from identity.
  void add(const DeviceLane& lane) {
    lanes_.push_back(std::make_unique<DeviceLane>(lane.device_id(),
                                                  /*stream_count=*/1,
                                                  lane.qos()));
  }

  DeviceLane* pick(int device_id, lane_qos min_qos) {
    const auto floor = static_cast<std::uint8_t>(min_qos);
    for (auto& lane : lanes_) {
      if (lane->device_id() != device_id) {
        continue;
      }
      if (static_cast<std::uint8_t>(lane->qos()) <= floor) {
        return lane.get();
      }
    }
    return nullptr;
  }

 private:
  std::vector<std::unique_ptr<DeviceLane>> lanes_;
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_LANE_SET_H
