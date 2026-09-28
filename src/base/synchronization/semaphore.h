// Copyright (c) 2022 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_SEMAPHORE_H
#define BASE_SYNCHRONIZATION_SEMAPHORE_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace base {
class semaphore {
 public:
  semaphore(long count = 0) : _counter(count) {}

 public:
  long count() const {
    std::lock_guard<std::mutex> lg{_lock};
    return _counter;
  }

  void wait() {
    std::unique_lock<std::mutex> ul{_lock};
    _cv.wait(ul, [this] { return (_counter > 0); });
    --_counter;
  }

  template <typename Rep, typename Period>
  std::cv_status wait_until(
      const std::chrono::duration<Rep, Period>& abs_time) {
    std::unique_lock<std::mutex> ul{_lock};
    while (_counter <= 0) {
      if (_cv.wait_until(ul, abs_time) == std::cv_status::timeout)
        return std::cv_status::timeout;
    }
    --_counter;
    return std::cv_status::no_timeout;
  }

  template <typename Rep, typename Period>
  std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    return wait_until(std::chrono::steady_clock::now() + rel_time);
  }

  void post(long count = 1) {
    std::unique_lock<std::mutex> ul{_lock};
    _counter += count;
    _cv.notify_all();
  }

 private:
  mutable std::mutex _lock;
  std::condition_variable _cv;
  long _counter;
};
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_SEMAPHORE_H
