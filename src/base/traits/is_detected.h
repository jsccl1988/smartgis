// Copyright (c) 2018 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TRAITS_IS_DETECTED_H
#define BASE_TRAITS_IS_DETECTED_H

#include <type_traits>

namespace base {
namespace detail {
struct nonesuch {
  nonesuch() = delete;
  ~nonesuch() = delete;
  nonesuch(const nonesuch&) = delete;
  void operator=(const nonesuch&) = delete;
};

template <class Default, class AlwaysVoid, template <class...> class Op,
          class... Args>
struct detector {
  using value_t = std::false_type;
  using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
  using value_t = std::true_type;
  using type = Op<Args...>;
};

}  // namespace detail

template <template <class...> class Op, class... Args>
using detected_t =
    typename detail::detector<detail::nonesuch, void, Op, Args...>::type;

template <template <class...> class Op, class... Args>
using is_detected =
    typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <class Expected, template <class...> class Op, class... Args>
using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

template <class To, template <class...> class Op, class... Args>
using is_detected_convertible =
    std::is_convertible<detected_t<Op, Args...>, To>;

template <class Default, template <class...> class Op, class... Args>
using detected_or = typename detail::detector<Default, void, Op, Args...>;

template <class Default, template <class...> class Op, class... Args>
using detected_or_t = typename detected_or<Default, Op, Args...>::type;
}  // namespace base
#endif  // BASE_TRAITS_IS_DETECTED_H
