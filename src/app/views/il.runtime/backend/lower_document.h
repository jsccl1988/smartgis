// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_SEMA_DOCUMENT_H_
#define IL_RUNTIME_EXECUTION_SEMA_DOCUMENT_H_

#include <optional>

#include "app/views/il.runtime/frontend/ast.h"
#include "app/views/il.runtime/backend/ops.h"

namespace app {
namespace detail {

// Document session: open, extent, style, export, detach, and edit tools.
std::optional<Action> try_lower_document_call(const CallStmt& c, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_SEMA_DOCUMENT_H_
