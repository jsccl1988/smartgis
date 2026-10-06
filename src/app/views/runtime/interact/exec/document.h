// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_EXEC_DOCUMENT_H_
#define APP_VIEWS_RUNTIME_INTERACT_EXEC_DOCUMENT_H_

#include <optional>

#include "app/views/runtime/interact/wire/ast.h"
#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// Document / map session verbs (sample, extent, style, export, detach).
std::optional<bool> try_exec_document_call(content::CapabilityHost& host,
                                           const CallStmt& c,
                                           VarMap* vars);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_EXEC_DOCUMENT_H_
