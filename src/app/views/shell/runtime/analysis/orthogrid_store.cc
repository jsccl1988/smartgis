// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/analysis/orthogrid_store.h"

#include <algorithm>

namespace app {

void OrthogridStore::clear() {
  nx_ = 0;
  ny_ = 0;
  xs_.clear();
  ys_.clear();
}

void OrthogridStore::begin(int expected_frames) {
  clear();
  xs_.reserve(static_cast<size_t>(std::max(1, expected_frames)));
  ys_.reserve(static_cast<size_t>(std::max(1, expected_frames)));
}

void OrthogridStore::push_frame(int nx,
                                int ny,
                                const double* xs,
                                const double* ys) {
  if (!xs || !ys || nx < 2 || ny < 2) {
    return;
  }
  const size_t n = static_cast<size_t>(nx) * static_cast<size_t>(ny);
  if (nx_ == 0) {
    nx_ = nx;
    ny_ = ny;
  } else if (nx != nx_ || ny != ny_) {
    return;
  }
  xs_.emplace_back(xs, xs + n);
  ys_.emplace_back(ys, ys + n);
}

const std::vector<double>* OrthogridStore::xs_at(int frame) const {
  if (frame < 0 || frame >= static_cast<int>(xs_.size())) {
    return nullptr;
  }
  return &xs_[static_cast<size_t>(frame)];
}

const std::vector<double>* OrthogridStore::ys_at(int frame) const {
  if (frame < 0 || frame >= static_cast<int>(ys_.size())) {
    return nullptr;
  }
  return &ys_[static_cast<size_t>(frame)];
}

}  // namespace app
