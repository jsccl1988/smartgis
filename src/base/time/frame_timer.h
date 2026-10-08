// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TIME_FRAME_TIMER_H_
#define BASE_TIME_FRAME_TIMER_H_

#include <chrono>

namespace base {

// Frame / wall clock helper used by legacy 3D scene (replaces Timer).
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
    write_clock(0, 0, 0);
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
    // Manual digits — avoid snprintf/CRT locks (UI timer + Display present
    // both call note_hud_frame and deadlocked on ucrtbased locks).
    write_clock(hh, mm, ss);
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
  void write_clock(int hh, int mm, int ss) {
    clock_buf_[0] = static_cast<char>('0' + (hh / 10) % 10);
    clock_buf_[1] = static_cast<char>('0' + hh % 10);
    clock_buf_[2] = ':';
    clock_buf_[3] = static_cast<char>('0' + (mm / 10) % 10);
    clock_buf_[4] = static_cast<char>('0' + mm % 10);
    clock_buf_[5] = ':';
    clock_buf_[6] = static_cast<char>('0' + (ss / 10) % 10);
    clock_buf_[7] = static_cast<char>('0' + ss % 10);
    clock_buf_[8] = '\0';
  }

  clock::time_point start_{};
  clock::time_point last_{};
  float elapsed_ = 0.0f;
  float stamp_ = 0.0f;
  float scale_ = 1.0f;
  char clock_buf_[16] = {};
};

}  // namespace base

#endif  // BASE_TIME_FRAME_TIMER_H_
