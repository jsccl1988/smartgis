// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/analysis/traffic_store.h"

#include <algorithm>

namespace app {

void TrafficStore::clear() {
  xy_.clear();
  cost_ = 0.0;
  network_path_.clear();
  frame_count_ = 0;
}

void TrafficStore::begin(std::vector<double> xy_interleaved,
                         double total_cost,
                         int frames,
                         std::string network_path) {
  xy_ = std::move(xy_interleaved);
  cost_ = total_cost;
  network_path_ = std::move(network_path);
  const int points = static_cast<int>(xy_.size() / 2);
  frame_count_ = std::max(1, frames);
  if (points < 2) {
    frame_count_ = 0;
  }
}

int TrafficStore::prefix_point_count(int frame_index) const {
  const int points = static_cast<int>(xy_.size() / 2);
  if (points < 2 || frame_count_ <= 0) {
    return 0;
  }
  const int f = frame_index + 1;
  return std::max(2, (points * f + frame_count_ - 1) / frame_count_);
}

}  // namespace app
