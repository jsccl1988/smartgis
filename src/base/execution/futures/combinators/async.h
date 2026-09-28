// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_FUTURES_COMBINATORS_ASYNC_H
#define BASE_EXECUTION_FUTURES_COMBINATORS_ASYNC_H

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include "base/core/debug.h"
#include "base/execution/futures/combinators/then.h"
#include "base/execution/futures/combinators/when_all.h"
#include "base/execution/futures/combinators/when_any.h"
#include "base/execution/executor/executor.h"
#include "base/memory/singleton.h"
#include "base/tuple/tuple.h"

namespace base {
namespace execution {
template <typename Executor, typename Fn, typename... Args>
auto async(Executor &&executor, Fn &&fn, Args &&...args) {
  return executor.execute(std::forward<Fn>(fn), std::forward<Args>(args)...);
}

namespace detail {
template <typename R, typename... Args>
struct bulk_context {
  std::function<R(size_t, Args...)> fn;
  std::tuple<Args...> args;
  const size_t total;
  const size_t bulk_size;
  alignas(
      base::hardware_destructive_interference_size) std::atomic<size_t> index;

  template <typename Fn, typename... Ts>
  explicit bulk_context(size_t total_, size_t bulk_size_, Fn &&fn_,
                        Ts &&...args_) noexcept
      : fn(std::forward<Fn>(fn_)),
        args(std::forward<Ts>(args_)...),
        total(total_),
        bulk_size(bulk_size_),
        index(0) {}

  inline size_t bulk_count() { return total / bulk_size + 1; }

  bool operator()() {
    size_t begin = index.fetch_add(bulk_size, std::memory_order_relaxed);
    size_t end = (std::min)(begin + bulk_size, total);
    for (size_t i = begin; i < end; ++i) {
      std::apply(fn, std::move(std::tuple_cat(std::forward_as_tuple(i), args)));
    }

    return true;
  }
  DISALLOW_COPY_AND_ASSIGN(bulk_context);
};

template <typename Fn, typename... Args>
inline auto make_bulk_context(size_t total, size_t bulk_size, Fn &&fn,
                              Args &&...args) noexcept {
  using R = std::invoke_result_t<Fn, size_t, Args...>;
  return std::make_shared<bulk_context<R, Args...>>(
      total, bulk_size, std::forward<Fn>(fn), std::forward<Args>(args)...);
}
}  // namespace detail

template <typename Executor, typename Fn, typename... Args>
auto bulk_async(Executor &&executor, size_t total, size_t bulk_size, Fn &&fn,
                Args &&...args) noexcept {
  if (bulk_size == 0) {
    bulk_size = 1;
  }

  using R = std::invoke_result_t<Fn, size_t, Args...>;
  auto context = detail::make_bulk_context(
      total, bulk_size, std::forward<Fn>(fn), std::forward<Args>(args)...);
  auto size = context->bulk_count();
  using FutureBool =
      typename std::decay_t<Executor>::ThreadContext::template Future<bool>;
  std::vector<FutureBool> futures;
  for (size_t i = 0; i < size; ++i) {
    futures.emplace_back(executor.execute([context]() { return (*context)(); }));
  }

  auto done = when_all(executor, futures.begin(), futures.end());
  return done;
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_FUTURES_COMBINATORS_ASYNC_H