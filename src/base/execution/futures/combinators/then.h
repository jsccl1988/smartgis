// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_FUTURES_COMBINATORS_THEN_H
#define BASE_EXECUTION_FUTURES_COMBINATORS_THEN_H

#include <vector>

#include "base/core/debug.h"
#include "base/execution/executor/executor.h"
#include "base/memory/singleton.h"
#include "base/tuple/tuple.h"

namespace base {
namespace execution {
template <typename Fu, typename Executor, typename Fn, typename... Args>
auto then(Fu &&future, Executor &&executor, Fn &&fn, Args &&...args) {
  using T = decltype(std::declval<Fu>().get());
  if constexpr (std::is_void_v<T>) {
    future.get();
    return executor.execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
  } else {
    return executor.execute(std::forward<Fn>(fn), std::move(future.get()),
                            std::forward<Args>(args)...);
  }
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_FUTURES_COMBINATORS_THEN_H