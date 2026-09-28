// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_THEN_H
#define BASE_EXECUTION_SENDER_RECEIVER_THEN_H

#include <vector>

#include "base/core/debug.h"
#include "base/execution/async/concepts/common.h"

namespace base {
namespace execution {
template <class R, class Fn>
struct then_receiver {
  R receiver;
  Fn fn;

  friend void set_value(then_receiver self, auto val) {
    set_value(self.receiver, self.fn(val));
  }

  friend void set_error(then_receiver self, std::exception_ptr err) {
    set_error(self.receiver, err);
  }

  friend void set_stopped(then_receiver self) { set_stopped(self.receiver); }
};

template <class S, class R, class Fn>
struct then_operation : immovable {
  connect_result_t<S, then_receiver<R, Fn>> op;

  friend void start(then_operation& self) { start(self.op); }
};

template <class S, class Fn>
struct then_sender {
  using result_t = std::invoke_result_t<Fn, sender_result_t<S>>;
  S s;
  Fn fn;

  template <class R>
  friend then_operation<S, R, Fn> connect(then_sender self, R receiver) {
    return {{}, connect(self.s, then_receiver<R, Fn>{receiver, self.fn})};
  }
};

template <class S, class Fn>
then_sender<S, Fn> then(S s, Fn fn) {
  return {s, fn};
}
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_THEN_H