// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Count-down latch that stores the first exception for parallel_for join.
// Uses std::mutex (not base::mutex). Std + this header only.

#ifndef BASE_EXECUTION_PARALLEL_DETAIL_COMPLETION_LATCH_H
#define BASE_EXECUTION_PARALLEL_DETAIL_COMPLETION_LATCH_H

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <utility>

namespace base {
namespace execution {
namespace detail {

class completion_latch {
 public:
  completion_latch() = default;
  completion_latch(const completion_latch&) = delete;
  completion_latch& operator=(const completion_latch&) = delete;

  void add(std::ptrdiff_t n) {
    if (n <= 0) {
      return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    counter_ += n;
  }

  void count_down() {
    std::lock_guard<std::mutex> lock(mu_);
    if (counter_ == 0) {
      return;
    }
    --counter_;
    if (counter_ == 0) {
      cv_.notify_all();
    }
  }

  void set_exception(std::exception_ptr e) {
    if (!e) {
      return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    if (!exception_) {
      exception_ = std::move(e);
    }
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return counter_ == 0; });
    if (exception_) {
      std::exception_ptr e = exception_;
      lock.unlock();
      std::rethrow_exception(e);
    }
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::ptrdiff_t counter_{0};
  std::exception_ptr exception_;
};

}  // namespace detail
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_PARALLEL_DETAIL_COMPLETION_LATCH_H
