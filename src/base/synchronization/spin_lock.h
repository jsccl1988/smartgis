// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_SPIN_LOCK_H
#define BASE_SYNCHRONIZATION_SPIN_LOCK_H

#include <atomic>
#include <thread>

#include "base/synchronization/spin_backoff.h"

namespace base {

// Lightweight mutex for very short critical sections. Prefer std::mutex
// (see mutex.mdc) when the critical section may block or run for long.
// Do not use base::mutex from synchronization/mutex.h in new code.
class spin_lock {
 public:
  void lock() {
    unsigned spins = 0;
    for (;;) {
      if (try_lock()) {
        return;
      }
      spin_backoff(spins);
    }
  }

  void unlock() { locked_.store(false, std::memory_order_release); }

  bool try_lock() {
    return !locked_.exchange(true, std::memory_order_acquire);
  }

 private:
  std::atomic<bool> locked_{false};
};

template <typename Lockable>
class scoped_spin_lock {
 public:
  explicit scoped_spin_lock(Lockable& lock) : lock_(lock) { lock_.lock(); }
  ~scoped_spin_lock() { lock_.unlock(); }

  scoped_spin_lock(const scoped_spin_lock&) = delete;
  scoped_spin_lock& operator=(const scoped_spin_lock&) = delete;

 private:
  Lockable& lock_;
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_SPIN_LOCK_H
