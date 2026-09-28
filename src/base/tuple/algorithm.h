// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TUPLE_ALGORITHM_H
#define BASE_TUPLE_ALGORITHM_H

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace base {
namespace tuple {

template <typename Tuple, typename Fn>
constexpr void for_each(Tuple&& tuple, Fn&& fn) {
  auto for_args = [&](auto&&... args) {
    return (void)(fn(std::forward<decltype(args)>(args)), ...);
  };
  std::apply(for_args, std::forward<Tuple>(tuple));
}

// Invokes fn.template operator()<I>(std::get<I>(tuple)) for each I.
template <typename Tuple, typename Fn>
constexpr void for_each_with_n(Tuple&& tuple, Fn&& fn) {
  [&]<std::size_t... Is>(auto&& t, std::index_sequence<Is...>) {
    (fn.template operator()<Is>(std::get<Is>(t)), ...);
  }(std::forward<Tuple>(tuple),
    std::make_index_sequence<
        std::tuple_size_v<std::decay_t<decltype(tuple)>>>{});
}

}  // namespace tuple
}  // namespace base

#endif  // BASE_TUPLE_ALGORITHM_H
