// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_NTHREAD_H
#define BASE_EXECUTION_NTHREAD_H

#include <algorithm>
#include <memory>
#include <vector>

#include "base/execution/executor/contexts/context.h"
#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/synchronization/align.h"
#include "base/synchronization/future.h"
#include "base/synchronization/latch.h"

namespace base {
namespace execution {
class NThreadPool
    : public ThreadContext<std::mutex, std::condition_variable, std::thread> {
 public:
  NThreadPool(std::size_t size = Thread::hardware_concurrency())
      : _size(size) {}
  ~NThreadPool() { join(); }

  template <typename Fn, typename... Args>
  [[nodiscard]] constexpr auto emplace(Fn&& fn, Args&&... args) noexcept {
    auto task =
        make_twoway_task(std::forward<Fn>(fn), std::forward<Args>(args)...);
    auto future = task->get_future();
    _tasks.push(task);
    return future;
  }

  size_t size() const { return _size; }

  void run() {
    for (std::size_t i = 0; i < _size; i++) {
      _threads.emplace_back([this] {
        while (true) {
          base::execution::Task* task{nullptr};
          if (LIKELY(_tasks.pop(task))) {
            (*task)();
            delete task;
          } else {
            break;
          }
        }
      });
    }
  }

  void join() {
    _tasks.close();
    std::for_each(_threads.begin(), _threads.end(), [](Thread& thread) {
      if (thread.joinable()) {
        thread.join();
      }
    });
  }

 private:
  std::size_t _size;
  std::vector<Thread> _threads;
  TaskQueue<base::execution::Task*, Mutex, ConditionVariable> _tasks;
  DISALLOW_COPY_AND_ASSIGN(NThreadPool);
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_NTHREAD_H
