// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_COLOR_H
#define SMT_LEGACY_CORE_COLOR_H

#include <algorithm>
#include <cmath>
#include <random>

#include "legacy/core/macros/macros.h"

inline long get_interp_color(long index, long internum, long r1, long g1,
                             long b1, long r2, long g2, long b2) {
  if (internum <= 0) {
    return RGB(r1, g1, b1);
  }
  index %= internum;
  const double t = static_cast<double>(index) / static_cast<double>(internum);
  const auto channel = [t](long a, long b) {
    return static_cast<int>(std::lround(a + (b - a) * t));
  };
  return RGB((std::clamp)(channel(r1, r2), 0, 255),
             (std::clamp)(channel(g1, g2), 0, 255),
             (std::clamp)(channel(b1, b2), 0, 255));
}

inline long get_random_color(void) {
  thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(1, 255);
  return RGB(dist(rng), dist(rng), dist(rng));
}

#endif  // SMT_LEGACY_CORE_COLOR_H
