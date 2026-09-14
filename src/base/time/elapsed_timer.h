// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TIME_ELAPSED_TIMER_H_
#define BASE_TIME_ELAPSED_TIMER_H_

#include <chrono>

namespace base {

// Steady-clock stopwatch (Phase 4 subset replacing SmtTimer frame clock).
class ElapsedTimer {
 public:
  using clock = std::chrono::steady_clock;

  ElapsedTimer() { reset(); }

  void reset() { start_ = clock::now(); }

  double elapsed_seconds() const {
    return std::chrono::duration<double>(clock::now() - start_).count();
  }

  double elapsed_milliseconds() const {
    return std::chrono::duration<double, std::milli>(clock::now() - start_)
        .count();
  }

 private:
  clock::time_point start_;
};

}  // namespace base

#endif  // BASE_TIME_ELAPSED_TIMER_H_
