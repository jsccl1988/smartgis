// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TIME_FRAME_TIMER_H_
#define BASE_TIME_FRAME_TIMER_H_

#include <chrono>
#include <cstdio>

namespace base {

// Frame / wall clock helper used by legacy 3D scene (replaces SmtTimer).
class FrameTimer {
 public:
  using clock = std::chrono::steady_clock;

  FrameTimer() { reset(); }

  void reset() {
    start_ = clock::now();
    last_ = start_;
    elapsed_ = 0.0f;
    stamp_ = 0.0f;
    scale_ = 1.0f;
    std::snprintf(clock_buf_, sizeof(clock_buf_), "00:00:00");
  }

  void update() {
    const auto now = clock::now();
    elapsed_ =
        std::chrono::duration<float>(now - last_).count();
    last_ = now;
    stamp_ = std::chrono::duration<float>(now - start_).count();
    const int total_sec = static_cast<int>(stamp_);
    const int hh = (total_sec / 3600) % 24;
    const int mm = (total_sec / 60) % 60;
    const int ss = total_sec % 60;
    std::snprintf(clock_buf_, sizeof(clock_buf_), "%02d:%02d:%02d", hh, mm, ss);
  }

  void set_clock(unsigned char /*hh*/, unsigned char /*mm*/) {
    // Compatibility no-op; update() refreshes the display clock from stamp.
  }

  const char* get_clock() const { return clock_buf_; }

  void set_scale(float factor) { scale_ = factor; }
  float get_scale() const { return scale_; }

  float get_time_stamp() const { return stamp_; }
  float get_elapsed() const { return elapsed_ * scale_; }
  float get_fps() const {
    return (elapsed_ > 0.0f) ? (1.0f / elapsed_) : 0.0f;
  }

 private:
  clock::time_point start_{};
  clock::time_point last_{};
  float elapsed_ = 0.0f;
  float stamp_ = 0.0f;
  float scale_ = 1.0f;
  char clock_buf_[16] = {};
};

}  // namespace base

#endif  // BASE_TIME_FRAME_TIMER_H_
