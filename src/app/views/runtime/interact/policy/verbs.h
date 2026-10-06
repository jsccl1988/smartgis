// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_POLICY_VERBS_H_
#define APP_VIEWS_RUNTIME_INTERACT_POLICY_VERBS_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "app/views/runtime/interact/wire/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

template <std::size_t N>
struct VerbEntry {
  std::array<std::string_view, N> names;
  std::optional<bool> (*fn)(content::CapabilityHost&, const CallStmt&, VarMap*);
};

template <typename Fn, typename... Names>
constexpr auto verb(Fn fn, Names... names) {
  return VerbEntry<sizeof...(Names)>{
      {std::string_view(names)...},
      fn,
  };
}

template <std::size_t N>
bool verb_matches(const VerbEntry<N>& e, std::string_view op) {
  for (std::string_view n : e.names) {
    if (n == op) {
      return true;
    }
  }
  return false;
}

template <typename... Entries>
std::optional<bool> dispatch_verbs(content::CapabilityHost& host,
                                   const CallStmt& c,
                                   VarMap* vars,
                                   const Entries&... entries) {
  std::optional<bool> hit;
  auto try_one = [&](const auto& e) {
    if (hit) {
      return;
    }
    if (verb_matches(e, c.name)) {
      hit = e.fn(host, c, vars);
    }
  };
  (try_one(entries), ...);
  return hit;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_POLICY_VERBS_H_
