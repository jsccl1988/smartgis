// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_PLUGIN_H_
#define IL_RUNTIME_CODEGEN_LOWER_PLUGIN_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/codegen/lower/ops.h"

namespace app {
namespace detail {

// Product scenario verbs (map2d_run, world3d_run, 鈥? plus processing / path /
// analysis / report CapabilityHost ops.
std::optional<Action> try_lower_plugin_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_PLUGIN_H_
