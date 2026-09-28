// Copyright (c) 2018 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_BARRIER_H
#define BASE_SYNCHRONIZATION_BARRIER_H

#include <stddef.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>

#include "base/synchronization/align.h"

namespace base {
template <typename T>
class barrier_base {
 public:
  using Complete = int (*)();
  barrier_base(size_t count)
      : _max_count{count}, _count{count}, _arrived_count{0} {}

  void arrive_and_wait() {
    std::unique_lock<std::mutex> ul{_lock};
    _allow.wait(ul, [this]() -> bool { return _count > 0; });

    if (--_count == 0) {
      ul.unlock();

      auto fn = static_cast<T*>(this)->complete();
      if (fn) {
        auto next_count = fn();
        if (next_count >= 0) {
          _max_count = next_count;
        }
      }

      _done.notify_all();
    } else {
      _done.wait(ul, [this]() -> bool { return _count == 0; });
      ul.unlock();
    }

    ul.lock();
    if (++_arrived_count == _max_count) {
      _count = _max_count;
      _arrived_count = 0;
      ul.unlock();
      _allow.notify_all();
    }
  }

 private:
  alignas(hardware_destructive_interference_size) size_t _max_count;
  std::mutex _lock;
  std::condition_variable _allow;
  std::condition_variable _done;
  alignas(hardware_destructive_interference_size) size_t _count;
  alignas(hardware_destructive_interference_size) size_t _arrived_count;
};

class barrier : public barrier_base<barrier> {
 public:
  barrier(size_t _count) : barrier_base{_count} {}
  Complete complete() { return nullptr; }
};

class flex_barrier : public barrier_base<flex_barrier> {
 public:
  flex_barrier(size_t count, Complete complete)
      : barrier_base{count}, _complete{complete} {}

  Complete complete() { return _complete; }

 private:
  Complete _complete;
};
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_BARRIER_H
