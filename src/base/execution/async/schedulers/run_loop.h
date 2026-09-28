// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_RUN_LOOP_H
#define BASE_EXECUTION_SENDER_RECEIVER_RUN_LOOP_H

#include <vector>

#include "base/core/debug.h"
#include "base/execution/executor/contexts/context.h"
#include "base/execution/async/concepts/common.h"

namespace base {
namespace execution {
struct run_loop : immovable {
  TaskQueue<Task*, std::mutex, std::condition_variable> tasks;

  struct none {};

  template <class R>
  struct operation : Task {
    R receiver;
    run_loop& loop;

    operation(R receiver, run_loop& loop) : receiver(receiver), loop(loop) {}

    virtual void operator()() override final { set_value(receiver, none{}); }

    friend void start(operation& self) { self.loop.tasks.push(&self); }
  };

  struct sender {
    using result_t = none;
    run_loop* loop;

    template <class R>
    friend operation<R> connect(sender self, R receiver) {
      return {receiver, *self.loop};
    }
  };

  struct scheduler {
    run_loop* loop;
    friend sender schedule(scheduler self) { return {self.loop}; }
  };

  scheduler get_scheduler() { return {this}; }

  void run() {
    base::execution::Task* task{nullptr};
    while (LIKELY(tasks.pop(task))) {
      (*task)();
    }
  }

  void finish() { tasks.close(); }
};
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_RUN_LOOP_H