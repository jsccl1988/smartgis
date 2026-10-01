// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_ANALYSIS_TRAFFIC_STORE_H_
#define APP_VIEWS_SHELL_RUNTIME_ANALYSIS_TRAFFIC_STORE_H_

#include <string>
#include <vector>

namespace app {

// Path geometry + cost for traffic shortest-path playback frames.
class TrafficStore {
 public:
  void clear();

  void begin(std::vector<double> xy_interleaved,
             double total_cost,
             int frames,
             std::string network_path);

  const std::vector<double>& xy() const { return xy_; }
  double cost() const { return cost_; }
  const std::string& network_path() const { return network_path_; }
  int frame_count() const { return frame_count_; }

  // Inclusive end vertex count for |frame_index| (at least 2 when valid).
  int prefix_point_count(int frame_index) const;

 private:
  std::vector<double> xy_;
  double cost_ = 0.0;
  std::string network_path_;
  int frame_count_ = 0;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_ANALYSIS_TRAFFIC_STORE_H_
