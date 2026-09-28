// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_JUST_H
#define BASE_EXECUTION_SENDER_RECEIVER_JUST_H

#include <vector>

#include "base/core/debug.h"
#include "base/execution/async/concepts/common.h"

namespace base {
namespace execution {
template <class R, class T>
struct just_operation : immovable {
  R receiver;
  T value;

  friend void start(just_operation& self) {
    set_value(self.receiver, self.value);
  }
};

template <class T>
struct just_sender {
  using result_t = T;
  T value;

  template <class R>
  friend just_operation<R, T> connect(just_sender self, R receiver) {
    return {{}, receiver, self.value};
  }
};

template <class T>
just_sender<T> just(T t) {
  return {t};
}
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_JUST_H