// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TUPLE_NAMED_TUPLE_H
#define BASE_TUPLE_NAMED_TUPLE_H

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/tuple/fixed_string.h"

namespace base {
namespace tuple {

template <fixed_string Name>
struct field_tag {
  static constexpr auto name = Name;

  template <typename T>
  constexpr auto operator=(T&& v) const;
};

template <fixed_string Name, typename T, bool Positional = true>
struct named {
  using value_type = T;
  static constexpr auto name = Name;
  static constexpr bool positional = Positional;
  T value{};
};

template <fixed_string Name>
template <typename T>
constexpr auto field_tag<Name>::operator=(T&& v) const {
  return named<Name, std::decay_t<T>, true>{std::forward<T>(v)};
}

template <fixed_string Name>
struct named_only_tag {
  static constexpr auto name = Name;

  template <typename T>
  constexpr auto operator=(T&& v) const {
    return named<Name, std::decay_t<T>, false>{std::forward<T>(v)};
  }
};

template <fixed_string Name>
inline constexpr named_only_tag<Name> named_only{};

template <fixed_string Name, typename... Fields>
inline constexpr std::size_t named_index = [] {
  constexpr bool hits[] = {(Fields::name == Name)...};
  for (std::size_t i = 0; i < sizeof...(Fields); ++i) {
    if (hits[i]) {
      return i;
    }
  }
  return static_cast<std::size_t>(-1);
}();

template <typename... Fields>
struct named_tuple {
  using is_named_tuple = void;
  std::tuple<Fields...> storage{};

  constexpr named_tuple() = default;
  constexpr named_tuple(Fields... fs) : storage(std::move(fs)...) {}

  template <fixed_string Name>
  constexpr auto& operator[](field_tag<Name>) & {
    static_assert(named_index<Name, Fields...> != static_cast<std::size_t>(-1),
                  "unknown named_tuple field");
    return std::get<named_index<Name, Fields...>>(storage).value;
  }

  template <fixed_string Name>
  constexpr const auto& operator[](field_tag<Name>) const& {
    static_assert(named_index<Name, Fields...> != static_cast<std::size_t>(-1),
                  "unknown named_tuple field");
    return std::get<named_index<Name, Fields...>>(storage).value;
  }

  template <fixed_string Name>
  constexpr auto& get() & {
    return (*this)[field_tag<Name>{}];
  }

  template <fixed_string Name>
  constexpr const auto& get() const& {
    return (*this)[field_tag<Name>{}];
  }
};

template <typename... Fields>
named_tuple(Fields...) -> named_tuple<Fields...>;

template <typename... Fields>
constexpr named_tuple<std::decay_t<Fields>...> make_named_tuple(
    Fields&&... fs) {
  return named_tuple<std::decay_t<Fields>...>{std::forward<Fields>(fs)...};
}

template <typename Tuple, typename Fn>
constexpr void for_each_named(Tuple&& t, Fn&& fn) {
  std::apply(
      [&](auto&&... fields) { (fn(std::forward<decltype(fields)>(fields)), ...); },
      std::forward<Tuple>(t).storage);
}

inline namespace literals {

template <fixed_string Name>
constexpr field_tag<Name> operator""_t() {
  return {};
}

}  // namespace literals

}  // namespace tuple

using tuple::make_named_tuple;
using tuple::named;
using tuple::named_only;
using tuple::named_tuple;

}  // namespace base

#endif  // BASE_TUPLE_NAMED_TUPLE_H
