// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Decorator executor: wraps an underlying executor and posts a vector of
// tasks as one batched work item. Not part of the L1 production surface.

#ifndef BASE_EXECUTION_EXECUTOR_POLICIES_BATCH_EXECUTOR_H
#define BASE_EXECUTION_EXECUTOR_POLICIES_BATCH_EXECUTOR_H

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "base/core/debug.h"
#include "base/execution/executor/contexts/context.h"
#include "base/execution/executor/pool/nthread_executor.h"

namespace base {
namespace execution {
template <typename UnderlyingExecutor = NThreadPoolExecutor>
class BatchExecutor {
 public:
  using ThreadContext = typename UnderlyingExecutor::ThreadContext;

  explicit BatchExecutor(
      std::size_t batch_size = 32,
      std::size_t threads = ThreadContext::Thread::hardware_concurrency())
      : _batch_size(batch_size),
        _underlying_executor(std::make_unique<UnderlyingExecutor>(threads)) {
    if (_batch_size == 0) {
      _batch_size = 1;
    }
  }

  explicit BatchExecutor(std::unique_ptr<UnderlyingExecutor> executor,
                         std::size_t batch_size = 32)
      : _batch_size(batch_size), _underlying_executor(std::move(executor)) {
    if (_batch_size == 0) {
      _batch_size = 1;
    }
  }

  ~BatchExecutor() {
    flush();
    _underlying_executor.reset();
  }

  template <typename Fn, typename... Args>
  constexpr auto execute(Fn &&fn, Args &&...args) noexcept {
    // For batching, we execute immediately but can batch internally
    // This maintains the standard executor interface
    return _underlying_executor->execute(std::forward<Fn>(fn),
                                         std::forward<Args>(args)...);
  }

  // Batch execution method - executes multiple tasks together
  template <typename Task>
  void post_batch(std::vector<Task> tasks) {
    if (tasks.empty()) {
      return;
    }

    // Create a batch task that executes all tasks
    auto batch_task = [tasks = std::move(tasks)]() mutable {
      for (auto &task : tasks) {
        task();
      }
    };

    _underlying_executor->execute(std::move(batch_task));
  }

  void flush() {
    // No-op for this implementation, but kept for interface compatibility
  }

  std::size_t batch_size() const noexcept { return _batch_size; }
  void set_batch_size(std::size_t size) noexcept {
    _batch_size = size;
    if (_batch_size == 0) {
      _batch_size = 1;
    }
  }

 private:
  std::size_t _batch_size;
  std::unique_ptr<UnderlyingExecutor> _underlying_executor;
  DISALLOW_COPY_AND_ASSIGN(BatchExecutor);
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_EXECUTOR_POLICIES_BATCH_EXECUTOR_H
