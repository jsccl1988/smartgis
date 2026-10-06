// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_EXEC_H_
#define APP_VIEWS_RUNTIME_INTERACT_EXEC_H_

#include "app/views/runtime/interact/wire/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

bool exec_stmt(content::CapabilityHost& host, const Stmt& stmt, VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_EXEC_H_
