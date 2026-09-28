// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_SYNCHRONIZATION_FUTEX_H
#define BASE_SYNCHRONIZATION_FUTEX_H

#include "base/core/build_config.h"

#if defined(OS_LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace base {
inline int futex_wait(void* addr, int block_if_value_is) {
  return static_cast<int>(syscall(SYS_futex, addr, FUTEX_WAIT, block_if_value_is,
                                  nullptr, nullptr, 0));
}

inline int futex_wake_one(void* addr) {
  return static_cast<int>(
      syscall(SYS_futex, addr, FUTEX_WAKE, 1, nullptr, nullptr, 0));
}

inline int futex_wake_all(void* addr) {
  return static_cast<int>(
      syscall(SYS_futex, addr, FUTEX_WAKE, 2147483647, nullptr, nullptr, 0));
}
}  // namespace base

#elif defined(OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace base {
inline int futex_wait(void* addr, int block_if_value_is) {
  return WaitOnAddress(addr, &block_if_value_is, sizeof(block_if_value_is),
                       INFINITE)
             ? 0
             : -1;
}

inline int futex_wake_one(void* addr) {
  WakeByAddressSingle(addr);
  return 1;
}

inline int futex_wake_all(void* addr) {
  WakeByAddressAll(addr);
  return 1;
}
}  // namespace base

#else
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace base {
namespace detail {
inline std::mutex& futex_fallback_mu() {
  static std::mutex mu;
  return mu;
}
inline std::condition_variable& futex_fallback_cv() {
  static std::condition_variable cv;
  return cv;
}
}  // namespace detail

inline int futex_wait(void* /*addr*/, int /*block_if_value_is*/) {
  std::unique_lock<std::mutex> lock(detail::futex_fallback_mu());
  detail::futex_fallback_cv().wait_for(lock, std::chrono::milliseconds(1));
  return 0;
}

inline int futex_wake_one(void* /*addr*/) {
  detail::futex_fallback_cv().notify_one();
  return 1;
}

inline int futex_wake_all(void* /*addr*/) {
  detail::futex_fallback_cv().notify_all();
  return 1;
}
}  // namespace base
#endif

#endif  // BASE_SYNCHRONIZATION_FUTEX_H
