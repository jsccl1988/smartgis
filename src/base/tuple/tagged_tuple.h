// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TUPLE_TAGGED_TUPLE_H
#define BASE_TUPLE_TAGGED_TUPLE_H

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace base {
namespace tuple {

template <typename Tag, typename T>
struct tagged {
  using tag_type = Tag;
  using value_type = T;
  T value{};
};

template <typename Tag, typename... Items>
inline constexpr std::size_t tagged_index = [] {
  constexpr bool hits[] = {std::is_same_v<typename Items::tag_type, Tag>...};
  for (std::size_t i = 0; i < sizeof...(Items); ++i) {
    if (hits[i]) {
      return i;
    }
  }
  return static_cast<std::size_t>(-1);
}();

template <typename... Items>
struct tagged_tuple {
  using is_tagged_tuple = void;
  std::tuple<Items...> storage{};

  constexpr tagged_tuple() = default;
  constexpr tagged_tuple(Items... xs) : storage(std::move(xs)...) {}
};

template <typename... Items>
tagged_tuple(Items...) -> tagged_tuple<Items...>;

template <typename Tag, typename... Items>
constexpr auto& get_tag(tagged_tuple<Items...>& t) {
  static_assert(tagged_index<Tag, Items...> != static_cast<std::size_t>(-1),
                "unknown tagged_tuple tag");
  return std::get<tagged_index<Tag, Items...>>(t.storage).value;
}

template <typename Tag, typename... Items>
constexpr const auto& get_tag(const tagged_tuple<Items...>& t) {
  static_assert(tagged_index<Tag, Items...> != static_cast<std::size_t>(-1),
                "unknown tagged_tuple tag");
  return std::get<tagged_index<Tag, Items...>>(t.storage).value;
}

template <typename Tag>
struct tag_resolver_t {
  template <typename T>
  constexpr tagged<Tag, std::decay_t<T>> operator=(T&& v) const {
    return tagged<Tag, std::decay_t<T>>{std::forward<T>(v)};
  }

  template <typename... Items>
  constexpr auto& operator()(tagged_tuple<Items...>& t) const {
    return get_tag<Tag>(t);
  }

  template <typename... Items>
  constexpr const auto& operator()(const tagged_tuple<Items...>& t) const {
    return get_tag<Tag>(t);
  }
};

template <typename Tag>
inline constexpr tag_resolver_t<Tag> tag_resolver{};

template <typename Tuple, typename Fn>
constexpr void for_each_tagged(Tuple&& t, Fn&& fn) {
  std::apply(
      [&](auto&&... items) { (fn(std::forward<decltype(items)>(items)), ...); },
      std::forward<Tuple>(t).storage);
}

}  // namespace tuple

using tuple::get_tag;
using tuple::tagged;
using tuple::tagged_tuple;
using tuple::tag_resolver;

}  // namespace base

#endif  // BASE_TUPLE_TAGGED_TUPLE_H
