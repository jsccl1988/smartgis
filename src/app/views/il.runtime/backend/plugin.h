// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_PLUGIN_H_
#define IL_RUNTIME_EXECUTION_SEMA_PLUGIN_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/backend/ops.h"

namespace app {
namespace detail {

// Product showcase ops (map2d_run, world3d_run, …) plus processing / path /
// analysis / report CapabilityHost ops.
std::optional<Action> try_lower_plugin_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_PLUGIN_H_
