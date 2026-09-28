// Copyright (c) 2018 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_LATCH_H
#define BASE_SYNCHRONIZATION_LATCH_H

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>

#include "base/core/log.h"
#include "base/synchronization/align.h"

namespace base {
class latch {
 public:
  latch(size_t count) : _count{count} {}

  void count_down() {
    std::unique_lock<std::mutex> ul{_lock};
    if (_count <= 0) {
      throw std::logic_error("latch count_down() called when count is already 0");
    }

    if (--_count == 0) {
      ul.unlock();
      _done.notify_all();
    }
  }

  void wait() {
    std::unique_lock<std::mutex> ul{_lock};
    if (_count > 0) {
      _done.wait(ul, [this]() -> bool { return _count == 0; });
    }
  }

 private:
  std::mutex _lock;
  std::condition_variable _done;
  alignas(hardware_destructive_interference_size) size_t _count;
};
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_LATCH_H
