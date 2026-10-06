// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_ACT_H_
#define IL_RUNTIME_EXECUTION_SEMA_ACT_H_

#include <type_traits>
#include <utility>

#include "app/views/il.runtime/backend/lower_bind.h"
#include "app/views/il.runtime/backend/ops.h"

namespace app {
namespace detail {

// Bind |schema| from the call now. The returned pack is what an Action may
// capture; it must not keep the CallStmt.
template <typename Pack>
Pack bind_pack(const CallStmt& c, Pack schema) {
  bind_into(c, schema);
  return schema;
}

// Run |project| at eval time. |project| returns the step result.
template <typename Pack, typename Fn>
std::optional<Action> host_action(Pack pack, Fn project) {
  return Action([pack = std::move(pack), project = std::move(project)](
                    content::CapabilityHost& host) {
    return project(host, pack);
  });
}

// Bind now, then project the pack. Same contract as host_action.
template <typename Pack, typename Fn>
std::optional<Action> bind_action(const CallStmt& c, Pack schema, Fn project) {
  return host_action(bind_pack(c, std::move(schema)), std::move(project));
}

// Zero-argument host op. A void IR call is a successful step.
template <auto Fn>
std::optional<Action> lower_host(const CallStmt&, VarMap*) {
  return Action([](content::CapabilityHost& host) {
    if constexpr (std::is_void_v<std::invoke_result_t<
                      decltype(Fn), content::CapabilityHost&>>) {
      Fn(host);
      return true;
    } else {
      return Fn(host);
    }
  });
}

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_ACT_H_
