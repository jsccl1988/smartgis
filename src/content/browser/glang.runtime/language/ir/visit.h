// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Runtime pick of a function object from a tuple, keyed by a static id.
// Uses base::tuple::for_each. Does not invent a class hierarchy per opcode.

#ifndef IL_RUNTIME_IR_VISIT_H_
#define IL_RUNTIME_IR_VISIT_H_

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/tuple/algorithm.h"

namespace content {
namespace ir {
namespace detail {

template <typename F>
concept id_tagged = requires { F::id; };

// Calls body with the first tuple element whose Fn::id equals key.
template <typename Tuple, typename Id, typename Body>
bool visit_id(Id key, Body&& body) {
  bool matched = false;
  base::tuple::for_each(Tuple{}, [&](auto sample) {
    using Fn = std::decay_t<decltype(sample)>;
    static_assert(id_tagged<Fn>, "tuple element is missing a static id");
    if (!matched && Fn::id == key) {
      body(Fn{});
      matched = true;
    }
    // Non-void so the comma fold in for_each stays well-formed.
    return 0;
  });
  return matched;
}

// Result of Fn::apply(args...). A static member function pointer is not a
// reliable std::invoke target on MSVC /std:c++23preview, so this names the
// call expression instead of decltype(&Fn::apply).
template <typename Fn, typename... Args>
using apply_result_t = decltype(Fn::apply(std::declval<Args>()...));

// Invokes Fn::apply for the matching id. nullopt means no element matched.
// Every element must share the apply signature of the first element.
template <typename Tuple, typename Id, typename... Args>
auto visit_apply(Id key, Args&&... args) {
  using Fn0 = std::tuple_element_t<0, Tuple>;
  using Result = apply_result_t<Fn0, Args...>;
  std::optional<Result> hit;
  visit_id<Tuple>(key, [&](auto sample) {
    using Fn = std::decay_t<decltype(sample)>;
    static_assert(std::is_same_v<apply_result_t<Fn, Args...>, Result>,
                  "tuple element apply() does not match the visit arguments");
    hit = Fn::apply(std::forward<Args>(args)...);
  });
  return hit;
}

}  // namespace detail
}  // namespace ir
}  // namespace content

#endif  // IL_RUNTIME_IR_VISIT_H_
