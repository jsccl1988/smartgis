// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_INPUT_H_
#define IL_RUNTIME_CODEGEN_LOWER_INPUT_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/codegen/lower/ops.h"

namespace app {
namespace detail {

// Pointer gestures (click, drag, wheel, path) and repeated bursts.
// Returns nullopt when |c.name| is not an input op.
std::optional<Action> try_lower_input_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_INPUT_H_
