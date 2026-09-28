// Copyright (c)  The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONCURRENCY_RCU_PTR_H
#define BASE_CONCURRENCY_RCU_PTR_H

#include <atomic>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace base {
// RCU (Read-Copy-Update) pointer implementation
// Requires C++20 for std::atomic<std::shared_ptr> support
template <typename T>
class rcu_ptr {
  // Use atomic shared_ptr for thread-safe read operations
  // Note: std::atomic<std::shared_ptr> is C++20 feature
  mutable std::atomic<std::shared_ptr<const T>> sp;

 public:
  rcu_ptr() : sp(nullptr) {}
  rcu_ptr(const rcu_ptr &) = delete;
  rcu_ptr &operator=(const rcu_ptr &) = delete;
  rcu_ptr(rcu_ptr &&) = delete;
  rcu_ptr &operator=(rcu_ptr &&) = delete;

  template <typename U>
  rcu_ptr(U &&sp_) : sp(std::forward<U>(sp_)) {}

  std::shared_ptr<const T> read() const {
    return sp.load(std::memory_order_acquire);
  }

  template <typename U>
  void reset(U &&r) {
    // Convert to shared_ptr<const T> for storage
    // Explicitly construct shared_ptr<const T> from the input
    // This handles both shared_ptr<T> and shared_ptr<const T> cases
    auto input_ptr = std::forward<U>(r);
    std::shared_ptr<const T> const_r(input_ptr);
    sp.store(std::move(const_r), std::memory_order_release);
  }

  template <typename R>
  void copy_update(R &&fun) {
    std::shared_ptr<const T> sp_l = sp.load(std::memory_order_acquire);

    std::shared_ptr<T> r;
    do {
      if (sp_l) {
        r = std::make_shared<T>(*sp_l);
      } else if constexpr (std::is_default_constructible_v<T>) {
        r = std::make_shared<T>();
      } else {
        throw std::logic_error(
            "rcu_ptr::copy_update requires an initial value for types that "
            "are not default constructible");
      }

      std::forward<R>(fun)(r.get());
    } while (!sp.compare_exchange_weak(
        sp_l, std::shared_ptr<const T>(std::move(r)),
        std::memory_order_release, std::memory_order_acquire));
  }
};
}  // namespace base

#endif  // BASE_CONCURRENCY_RCU_PTR_H