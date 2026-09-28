// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_THREAD_CONTEXT_H
#define BASE_EXECUTION_SENDER_RECEIVER_THREAD_CONTEXT_H

#include <vector>

#include "base/core/debug.h"
#include "base/execution/async/concepts/common.h"
#include "base/execution/async/schedulers/run_loop.h"

namespace base {
namespace execution {
struct thread_context {
  thread_context() noexcept : thread{[this] { loop.run(); }} {}
  ~thread_context() noexcept { join(); }

  void join() noexcept {
    loop.finish();
    thread.join();
  }

  auto get_scheduler() { return loop.get_scheduler(); }

 private:
  std::thread thread;
  run_loop loop;
};
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_THREAD_CONTEXT_H