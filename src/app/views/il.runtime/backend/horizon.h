// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_HORIZON_H_
#define IL_RUNTIME_EXECUTION_SEMA_HORIZON_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/backend/ops.h"

namespace app {
namespace detail {

// Shell clock, chrome, scenario, capture, viewport wait, and debug console.
// Testing IL owns step order. Expect gates lower in expect.h.
std::optional<Action> try_lower_horizon_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_HORIZON_H_
