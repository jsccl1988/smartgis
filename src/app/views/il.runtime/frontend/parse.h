// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_EXECUTION_FRONT_PARSE_H_
#define IL_RUNTIME_EXECUTION_FRONT_PARSE_H_

#include <string>

#include "app/views/il.runtime/frontend/ast.h"

namespace app {
namespace detail {

// Parses Interact.g4 source into |out|. On failure writes a message to |err|.
bool parse_interact_source(const std::string& src,
                           ScriptAst* out,
                           std::string* err);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_EXECUTION_FRONT_PARSE_H_
