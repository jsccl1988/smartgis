// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_ANALYSIS_ORTHOGRID_STORE_H_
#define APP_VIEWS_SHELL_RUNTIME_ANALYSIS_ORTHOGRID_STORE_H_

#include <vector>

namespace app {

// Orthogrid / orthogrid3d node-coordinate frames for ResultPlayback.
class OrthogridStore {
 public:
  void clear();

  void begin(int expected_frames);

  void push_frame(int nx, int ny, const double* xs, const double* ys);

  int nx() const { return nx_; }
  int ny() const { return ny_; }
  int frame_count() const { return static_cast<int>(xs_.size()); }

  const std::vector<double>* xs_at(int frame) const;
  const std::vector<double>* ys_at(int frame) const;

 private:
  int nx_ = 0;
  int ny_ = 0;
  std::vector<std::vector<double>> xs_;
  std::vector<std::vector<double>> ys_;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_ANALYSIS_ORTHOGRID_STORE_H_
