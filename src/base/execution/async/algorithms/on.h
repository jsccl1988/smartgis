// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_ON_H
#define BASE_EXECUTION_SENDER_RECEIVER_ON_H

#include <type_traits>
#include <utility>

#include "base/core/debug.h"
#include "base/execution/async/concepts/common.h"
#include "base/execution/async/schedulers/run_loop.h"
#include "base/execution/async/algorithms/then.h"

namespace base {
namespace execution {

// Forward declaration
template <class Sch>
auto schedule(Sch sch);

template <class Sch, class R>
struct on_receiver {
  Sch scheduler;
  R receiver;

  template <class Val>
  friend void set_value(on_receiver self, Val&& val) {
    // Schedule the value delivery on the scheduler
    auto sched = schedule(self.scheduler);
    auto receiver = std::move(self.receiver);
    auto s = then(sched, [val = std::forward<Val>(val),
                          receiver = std::move(receiver)](auto) mutable {
      set_value(receiver, std::move(val));
    });
    // Use a dummy receiver for the then operation
    struct dummy_receiver {
      friend void set_value(dummy_receiver, auto) {}
      friend void set_error(dummy_receiver, std::exception_ptr) {}
      friend void set_stopped(dummy_receiver) {}
    };
    auto op = connect(s, dummy_receiver{});
    start(op);
  }

  friend void set_error(on_receiver self, std::exception_ptr err) {
    // Schedule the error delivery on the scheduler
    auto sched = schedule(self.scheduler);
    auto receiver = std::move(self.receiver);
    auto s = then(sched, [err, receiver = std::move(receiver)](auto) mutable {
      set_error(receiver, err);
    });
    struct dummy_receiver {
      friend void set_value(dummy_receiver, auto) {}
      friend void set_error(dummy_receiver, std::exception_ptr) {}
      friend void set_stopped(dummy_receiver) {}
    };
    auto op = connect(s, dummy_receiver{});
    start(op);
  }

  friend void set_stopped(on_receiver self) {
    // Schedule the stopped delivery on the scheduler
    auto sched = schedule(self.scheduler);
    auto receiver = std::move(self.receiver);
    auto s = then(sched, [receiver = std::move(receiver)](auto) mutable {
      set_stopped(receiver);
    });
    struct dummy_receiver {
      friend void set_value(dummy_receiver, auto) {}
      friend void set_error(dummy_receiver, std::exception_ptr) {}
      friend void set_stopped(dummy_receiver) {}
    };
    auto op = connect(s, dummy_receiver{});
    start(op);
  }
};

template <class Sch, class S, class R>
struct on_operation : immovable {
  connect_result_t<S, on_receiver<Sch, R>> op;

  friend void start(on_operation& self) { start(self.op); }
};

template <class Sch, class S>
struct on_sender {
  using result_t = sender_result_t<S>;
  Sch scheduler;
  S sender;

  template <class R>
  friend on_operation<Sch, S, R> connect(on_sender self, R receiver) {
    return {{}, connect(std::move(self.sender),
                        on_receiver<Sch, R>{std::move(self.scheduler),
                                            std::move(receiver)})};
  }
};

template <class Sch, class S>
constexpr on_sender<std::decay_t<Sch>, std::decay_t<S>> on(Sch&& sch, S&& s) {
  return {std::forward<Sch>(sch), std::forward<S>(s)};
}

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_ON_H

