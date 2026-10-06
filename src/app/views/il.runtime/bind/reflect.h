// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BIND_REFLECT_H_
#define IL_RUNTIME_BIND_REFLECT_H_

#include <type_traits>
#include <utility>

#include "base/tuple/tuple.h"

namespace app {
namespace detail {

// named_tuple / tagged_tuple are this repo's mogu aggregate reflection surface
// (field iteration via for_each_named / for_each_tagged). There is no second
// Boost.PFR tree.

template <typename T>
concept named_aggregate = requires {
  typename std::remove_cvref_t<T>::is_named_tuple;
};

template <typename T>
concept tagged_aggregate = requires {
  typename std::remove_cvref_t<T>::is_tagged_tuple;
};

template <typename T>
concept reflected_pack = named_aggregate<T> || tagged_aggregate<T>;

template <named_aggregate Tuple, typename Fn>
constexpr void reflect_fields(Tuple&& pack, Fn&& fn) {
  base::tuple::for_each_named(std::forward<Tuple>(pack), std::forward<Fn>(fn));
}

template <tagged_aggregate Tuple, typename Fn>
constexpr void reflect_fields(Tuple&& pack, Fn&& fn) {
  base::tuple::for_each_tagged(std::forward<Tuple>(pack), std::forward<Fn>(fn));
}

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BIND_REFLECT_H_
