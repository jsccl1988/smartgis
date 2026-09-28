// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ALL_H
#define BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ALL_H

#include <iterator>
#include <utility>
#include <vector>
#include <variant>

#include "base/core/debug.h"
#include "base/execution/executor/executor.h"
#include "base/execution/futures/combinators/then.h"
#include "base/memory/singleton.h"
#include "base/tuple/tuple.h"

namespace base {
namespace execution {

namespace detail {
// Helper to map void to std::monostate for vector storage
template <typename T>
struct non_void_type {
  using type = T;
};

template <>
struct non_void_type<void> {
  using type = std::monostate;
};

template <typename T>
using non_void_t = typename non_void_type<T>::type;
}  // namespace detail

template <typename Executor, typename Iterator>
inline auto when_all(Executor &executor, Iterator begin, Iterator end) {
  using Fu = typename std::iterator_traits<Iterator>::value_type;
  using T = std::decay_t<decltype(std::declval<Fu>().get())>;
  using StorageT = detail::non_void_t<T>;

  if (begin == end) {
    return make_ready_future<
        std::vector<StorageT>, typename Executor::ThreadContext::Mutex,
        typename Executor::ThreadContext::ConditionVariable>({});
  }

  struct context {
    context(int n) : results(n) {}
    using PromiseVec =
        typename Executor::ThreadContext::template Promise<std::vector<StorageT>>;
    PromiseVec promise;
    std::vector<StorageT> results;
    size_t count{0};
    typename Executor::ThreadContext::Mutex mtx;
  };

  auto ctx = std::make_shared<context>(std::distance(begin, end));
  for (size_t i = 0; begin != end; ++begin, ++i) {
    if constexpr (std::is_void_v<T>) {
      then(*begin, executor, [ctx, i]() {
        std::unique_lock<typename Executor::ThreadContext::Mutex> lock(ctx->mtx);
        ctx->results[i] = std::monostate{};
        ctx->count++;
        if (ctx->results.size() == ctx->count) {
          ctx->promise.set_value(std::move(ctx->results));
        }

        return true;
      });
    } else {
      then(*begin, executor, [ctx, i](T t) {
        std::unique_lock<typename Executor::ThreadContext::Mutex> lock(ctx->mtx);
        ctx->results[i] = std::move(t);
        ctx->count++;
        if (ctx->results.size() == ctx->count) {
          ctx->promise.set_value(std::move(ctx->results));
        }

        return true;
      });
    }
  }

  return ctx->promise.get_future();
}

namespace detail {
template <typename Ex, typename... Fu>
class when_all_context
    : public std::enable_shared_from_this<when_all_context<Ex, Fu...>> {
 public:
  using R = std::tuple<
      std::decay_t<decltype(std::declval<std::decay_t<Fu>>().get())>...>;
  using Executor = std::decay_t<Ex>;
  using P = typename Executor::ThreadContext::template Promise<R>;
  using M = typename Executor::ThreadContext::Mutex;

  when_all_context(Executor &exe) : executor(exe) {}

  auto get_future() { return promise.get_future(); }

  template <typename Tuple>
  void for_each(Tuple &&tuple) {
    base::tuple::for_each_with_n(std::forward<Tuple>(tuple), *this);
  }

  template <std::size_t I>
  void operator()(auto &&f) {
    using F = std::decay_t<decltype(f)>;
    using value_t = std::decay_t<decltype(std::declval<F>().get())>;
    auto self = this->shared_from_this();
    then(f, executor, [self, this](value_t t) mutable {
      std::unique_lock<typename Executor::ThreadContext::Mutex> lock(mutex);
      count++;
      std::get<I>(results) = std::move(t);
      if (count == sizeof...(Fu)) {
        promise.set_value(std::move(results));
      }

      return true;
    });
  }

 private:
  Executor &executor;
  P promise;
  R results;
  M mutex;
  size_t count = 0;
};
}  // namespace detail

template <typename Executor, typename... Fu>
auto when_all_variadic(Executor &&executor, Fu &&...futures) {
  auto ctx =
      std::make_shared<detail::when_all_context<Executor, Fu...>>(executor);
  ctx->for_each(std::forward_as_tuple(std::forward<Fu>(futures)...));
  return ctx->get_future();
}
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_FUTURES_COMBINATORS_WHEN_ALL_H