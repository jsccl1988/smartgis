// Copyright (c) 2022 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_WAITER_H
#define BASE_SYNCHRONIZATION_WAITER_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace base {
enum class wait_status { Resting, Arrived, Excited };
class waiter {
 public:
  wait_status status() const {
    std::lock_guard<std::mutex> lg{_lock};
    return signaled_;
  }

  bool is_signaled() const {
    std::lock_guard<std::mutex> lg{_lock};
    return (signaled_ != wait_status::Resting);
  }

  void reset() {
    std::unique_lock<std::mutex> ul{_lock};
    signaled_ = wait_status::Resting;
  }

 public:
  void wait() {
    std::unique_lock<std::mutex> ul{_lock};
    _cv.wait(ul, [this] { return (signaled_ != wait_status::Resting); });

    if (signaled_ == wait_status::Arrived) {
      signaled_ = wait_status::Resting;
    }
  }

  template <typename Rep, typename Period>
  std::cv_status wait_until(
      const std::chrono::duration<Rep, Period>& abs_time) {
    std::unique_lock<std::mutex> ul{_lock};
    while (signaled_ == wait_status::Resting) {
      if (_cv.wait_until(ul, abs_time) == std::cv_status::timeout)
        return std::cv_status::timeout;
    }

    if (signaled_ == wait_status::Arrived) {
      signaled_ = wait_status::Resting;
    }

    return std::cv_status::no_timeout;
  }

  template <typename Rep, typename Period>
  std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    return wait_until(std::chrono::steady_clock::now() + rel_time);
  }

  void notify_one() {
    std::lock_guard<std::mutex> lg{_lock};
    signaled_ = wait_status::Arrived;
    _cv.notify_one();
  }

  void notify_all() {
    std::lock_guard<std::mutex> lg{_lock};
    signaled_ = wait_status::Excited;
    _cv.notify_all();
  }

 private:
  mutable std::mutex _lock;
  std::condition_variable _cv;
  wait_status signaled_ = wait_status::Resting;
};
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_WAITER_H
