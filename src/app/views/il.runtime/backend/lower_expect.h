// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_EXPECT_H_
#define IL_RUNTIME_EXECUTION_SEMA_EXPECT_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/backend/ops.h"

namespace app {
namespace detail {

// Expect gates only: expect_tool, expect_geom, and expect_* probes.
// Tool commands lower in document; viewport wait and debug console in horizon.
std::optional<Action> try_lower_expect_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_EXPECT_H_
