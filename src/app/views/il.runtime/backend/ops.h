// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_OPS_H_
#define IL_RUNTIME_EXECUTION_SEMA_OPS_H_

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>

#include "app/views/il.runtime/frontend/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// Bound semantic action. Eval runs it. Closures may hold VarMap* for $var
// and as= writes; they must not hold CallStmt.
using Action = std::function<bool(content::CapabilityHost&)>;

// One operator (source names) plus its lowering rule.
template <std::size_t N>
struct OpEntry {
  std::array<std::string_view, N> names;
  std::optional<Action> (*fn)(const CallStmt&, VarMap*);
};

template <typename Fn, typename... Names>
constexpr auto op(Fn fn, Names... names) {
  return OpEntry<sizeof...(Names)>{
      {std::string_view(names)...},
      fn,
  };
}

template <std::size_t N>
bool op_matches(const OpEntry<N>& e, std::string_view name) {
  for (std::string_view n : e.names) {
    if (n == name) {
      return true;
    }
  }
  return false;
}

// Match an op by CallStmt.name and lower it now (bind_into).
// The Action must not see AST.
template <typename... Entries>
std::optional<Action> lower_ops(const CallStmt& c,
                                VarMap* vars,
                                const Entries&... entries) {
  std::optional<Action> hit;
  auto try_one = [&](const auto& e) {
    if (hit || !op_matches(e, c.name)) {
      return;
    }
    hit = e.fn(c, vars);
  };
  (try_one(entries), ...);
  return hit;
}

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_OPS_H_
