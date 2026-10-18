// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Pick a stateless function object from a tuple by its static id.
// Kernels live under gis::detail and cannot include the glang visit header.
// The call shape matches content::ir::detail::visit_apply.

#ifndef GIS_ANALYSIS_OPS_IR_VISIT_H_
#define GIS_ANALYSIS_OPS_IR_VISIT_H_

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/tuple/algorithm.h"

namespace gis {
namespace detail {

template <typename F>
concept ir_id_tagged = requires { F::id; };

// nullopt means no tuple element carried this id.
template <typename Tuple, typename Id, typename... Args>
auto visit_apply(Id key, Args&&... args) {
  using Fn0 = std::tuple_element_t<0, Tuple>;
  using Result = decltype(Fn0::apply(std::declval<Args>()...));
  std::optional<Result> hit;
  bool matched = false;
  base::tuple::for_each(Tuple{}, [&](auto sample) {
    using Fn = std::decay_t<decltype(sample)>;
    static_assert(ir_id_tagged<Fn>, "tuple element is missing a static id");
    if (!matched && key == Fn::id) {
      static_assert(std::is_same_v<decltype(Fn::apply(std::declval<Args>()...)), Result>,
                    "tuple element apply() does not match the visit arguments");
      hit = Fn::apply(std::forward<Args>(args)...);
      matched = true;
    }
    return 0;
  });
  return hit;
}

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_IR_VISIT_H_
