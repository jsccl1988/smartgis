// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_SYNC_H
#define BASE_EXECUTION_SENDER_RECEIVER_SYNC_H

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>

#include "base/core/debug.h"
#include "base/execution/async/concepts/common.h"

namespace base {
namespace execution {
struct sync_waiter {
  std::mutex mtx;
  std::condition_variable cv;
  std::exception_ptr err;
  bool done = false;
};

template <class T>
struct sync_receiver {
  sync_waiter& waiter;
  std::optional<T>& value;

  friend void set_value(sync_receiver self, auto val) {
    std::unique_lock lk{self.waiter.mtx};
    self.value.emplace(val);
    self.waiter.done = true;
    self.waiter.cv.notify_one();
  }

  friend void set_error(sync_receiver self, std::exception_ptr err) {
    std::unique_lock lk{self.waiter.mtx};
    self.waiter.err = err;
    self.waiter.done = true;
    self.waiter.cv.notify_one();
  }

  friend void set_stopped(sync_receiver self) {
    std::unique_lock lk{self.waiter.mtx};
    self.waiter.done = true;
    self.waiter.cv.notify_one();
  }
};

template <class S>
std::optional<sender_result_t<S>> sync(S s) {
  using T = sender_result_t<S>;
  sync_waiter waiter;
  std::optional<T> value;

  auto op = connect(s, sync_receiver<T>{waiter, value});
  start(op);

  std::unique_lock lk{waiter.mtx};
  waiter.cv.wait(lk, [&] { return waiter.done; });

  if (waiter.err) {
    std::rethrow_exception(waiter.err);
  }

  return value;
}
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_SYNC_H