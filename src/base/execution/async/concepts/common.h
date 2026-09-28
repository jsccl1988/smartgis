// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_SENDER_RECEIVER_COMMON_H
#define BASE_EXECUTION_SENDER_RECEIVER_COMMON_H

#include <vector>

#include "base/core/debug.h"

namespace base {
namespace execution {
struct immovable {
  immovable() = default;
  immovable(immovable&&) = delete;
};

template <class S, class R>
using connect_result_t =
    decltype(connect(std::declval<S>(), std::declval<R>()));

template <class S>
using sender_result_t = typename S::result_t;
}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_SENDER_RECEIVER_COMMON_H