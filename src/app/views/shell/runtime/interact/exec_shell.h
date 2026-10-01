// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_INTERACT_EXEC_SHELL_H_
#define APP_VIEWS_SHELL_RUNTIME_INTERACT_EXEC_SHELL_H_

#include <optional>

#include "app/views/shell/runtime/interact/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// Host / shell / analysis / path CapabilityHost verbs.
// Returns nullopt when |c.name| is not a shell verb (caller tries input path).
std::optional<bool> try_exec_shell_call(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_INTERACT_EXEC_SHELL_H_
