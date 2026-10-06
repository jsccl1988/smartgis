// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_VIEW_H_
#define IL_RUNTIME_CODEGEN_LOWER_VIEW_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/codegen/lower/ops.h"

namespace app {
namespace detail {

// View lane: present, tools, viewport wait, and view gates.
std::optional<Action> try_lower_view_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_VIEW_H_
