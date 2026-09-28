// Copyright (c) 2018 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TRAITS_CONCEPT_H
#define BASE_TRAITS_CONCEPT_H

#include <type_traits>

#include "base/traits/is_detected.h"

namespace base {
template <bool... Bs>
constexpr bool require = std::conjunction<std::bool_constant<Bs>...>::value;

template <bool... Bs>
constexpr bool either = std::disjunction<std::bool_constant<Bs>...>::value;

template <bool... Bs>
constexpr bool disallow = not require<Bs...>;

template <template <class...> class Op, class... Args>
constexpr bool exists = is_detected<Op, Args...>::value;

template <class To, template <class...> class Op, class... Args>
constexpr bool converts_to = is_detected_convertible<To, Op, Args...>::value;

template <class Exact, template <class...> class Op, class... Args>
constexpr bool identical_to = is_detected_exact<Exact, Op, Args...>::value;

#define REQUIRES(...) std::enable_if<(__VA_ARGS__)>
}  // namespace base
#endif  // BASE_TRAITS_CONCEPT_H
