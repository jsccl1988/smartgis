// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_LOWER_H_
#define IL_RUNTIME_CODEGEN_LOWER_LOWER_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/codegen/lower/code.h"

namespace app {
namespace detail {

// Lower one call to a bound action. nullopt means the op is unknown.
// Binds arguments now. Does not touch CapabilityHost.
std::optional<Action> lower_call(const CallStmt& call, VarMap* vars);

// Lower a statement / whole script. Unknown ops become kNop (skip).
bool lower_stmt(const Stmt& stmt, VarMap* vars, Code* out);
Code lower_script(const ScriptAst& ast, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_LOWER_H_
