// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_POLICY_BIND_H_
#define APP_VIEWS_RUNTIME_INTERACT_POLICY_BIND_H_

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "app/views/runtime/interact/policy/args.h"
#include "base/tuple/tuple.h"

namespace app {
namespace detail {

using base::make_named_tuple;
using base::named_only;
using namespace base::tuple::literals;

// Fill a named_tuple from CallStmt: named lookup first, then positional for
// fields with named<>::positional == true (named_only fields skip the index).
template <typename... Fields>
void bind_into(const CallStmt& c, base::named_tuple<Fields...>& out) {
  int pos = 0;
  auto bind_one = [&]<typename F>(F& field) {
    constexpr std::string_view nm = F::name.view();
    using T = typename F::value_type;
    const int positional = F::positional ? pos : -1;
    if constexpr (std::is_same_v<T, int>) {
      field.value = arg_int(c, positional, nm, field.value);
    } else if constexpr (std::is_same_v<T, std::string>) {
      const std::string def = field.value;
      field.value = arg_ident(c, positional, nm, def);
    } else if constexpr (std::is_same_v<T, std::vector<Point>>) {
      field.value = arg_points(c);
    }
    if constexpr (F::positional) {
      ++pos;
    }
  };
  std::apply([&](auto&... fs) { (bind_one(fs), ...); }, out.storage);
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_POLICY_BIND_H_
