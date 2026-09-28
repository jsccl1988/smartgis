// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ANY_H
#define BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ANY_H

#include <atomic>
#include <iterator>
#include <utility>
#include <variant>
#include <vector>

#include "base/core/debug.h"
#include "base/execution/executor/executor.h"
#include "base/execution/futures/combinators/then.h"
#include "base/memory/singleton.h"
#include "base/tuple/tuple.h"

namespace base {
namespace execution {
template <typename Executor, typename Iterator>
inline auto when_any(Executor& executor, Iterator begin, Iterator end) {
  using Fu = typename std::iterator_traits<Iterator>::value_type;
  using T = std::decay_t<decltype(std::declval<Fu>().get())>;

  if (begin == end) {
    return make_ready_future<
        std::pair<size_t, T>, typename Executor::ThreadContext::Mutex,
        typename Executor::ThreadContext::ConditionVariable>({});
  }

  struct context {
    using PromisePair =
        typename Executor::ThreadContext::template Promise<std::pair<size_t, T>>;
    PromisePair promise;
    std::atomic<bool> done{false};
  };

  auto ctx = std::make_shared<context>();
  for (size_t i = 0; begin != end; ++begin, ++i) {
    then(*begin, executor, [ctx, i](T t) {
      if (!ctx->done.exchange(true)) {
        ctx->promise.set_value(std::make_pair(i, std::move(t)));
      }
      return true;
    });
  }

  return ctx->promise.get_future();
}

namespace detail {
template <typename Ex, typename... Fu>
class when_any_context
    : public std::enable_shared_from_this<when_any_context<Ex, Fu...>> {
 public:
  using R =
      std::pair<size_t,
                std::variant<std::decay_t<
                    decltype(std::declval<std::decay_t<Fu>>().get())>...>>;
  using Executor = std::decay_t<Ex>;
  using P = typename Executor::ThreadContext::template Promise<R>;
  using M = typename Executor::ThreadContext::Mutex;

  when_any_context(Executor& exe) : executor(exe) {}

  auto get_future() { return promise.get_future(); }

  template <typename Tuple>
  void for_each(Tuple&& tuple) {
    base::tuple::for_each_with_n(std::forward<Tuple>(tuple), *this);
  }

  template <std::size_t I>
  void operator()(auto&& f) {
    using F = std::decay_t<decltype(f)>;
    using value_t = std::decay_t<decltype(std::declval<F>().get())>;
    auto self = this->shared_from_this();
    then(f, executor, [self, this](value_t t) {
      std::unique_lock<typename Executor::ThreadContext::Mutex> lock(mutex);
      if (has_value) {
        return true;
      }

      has_value = true;
      results = std::make_pair(static_cast<size_t>(I), std::move(t));
      promise.set_value(std::move(results));
      return true;
    });
  }

 private:
  Executor& executor;
  P promise;
  R results;
  M mutex;
  bool has_value = false;
};
}  // namespace detail

template <typename Executor, typename... Fu>
auto when_any_variadic(Executor&& executor, Fu&&... futures) {
  auto ctx =
      std::make_shared<detail::when_any_context<Executor, Fu...>>(executor);
  ctx->for_each(std::forward_as_tuple(std::forward<Fu>(futures)...));
  return ctx->get_future();
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ANY_H
