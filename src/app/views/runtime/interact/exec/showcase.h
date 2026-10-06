// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_EXEC_SHOWCASE_H_
#define APP_VIEWS_RUNTIME_INTERACT_EXEC_SHOWCASE_H_

#include <optional>

#include "app/views/runtime/interact/wire/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// Tool / digitize / suite-runner verbs (map2d_run, atmosphere_run, …).
std::optional<bool> try_exec_showcase_call(content::CapabilityHost& host,
                                           const CallStmt& c,
                                           VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_EXEC_SHOWCASE_H_
