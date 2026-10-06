// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_EVAL_EXEC_H_
#define IL_RUNTIME_CODEGEN_EVAL_EXEC_H_

#include "app/views/il.runtime/codegen/lower/code.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// Run lowered code. No AST / CallStmt.
bool eval_code(content::CapabilityHost& host, const Code& code);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_EVAL_EXEC_H_
