// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Lightweight spin → yield for short waits (queue backpressure, etc.).
// Not a mutex substitute; do not use for long or blocking critical sections.

#ifndef BASE_SYNCHRONIZATION_SPIN_BACKOFF_H
#define BASE_SYNCHRONIZATION_SPIN_BACKOFF_H

#include <thread>

namespace base {

// Bump spins; yield every 64 iterations so busy-wait does not pin a core.
inline void spin_backoff(unsigned& spins) noexcept {
  ++spins;
  if ((spins & 63u) == 0u) {
    std::this_thread::yield();
  }
}

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_SPIN_BACKOFF_H
