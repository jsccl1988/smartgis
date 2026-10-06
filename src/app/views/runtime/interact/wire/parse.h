// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_WIRE_PARSE_H_
#define APP_VIEWS_RUNTIME_INTERACT_WIRE_PARSE_H_

#include <string>

#include "app/views/runtime/interact/wire/ast.h"

namespace app {
namespace detail {

// Parses Interact.g4 source into |out|. On failure writes a message to |err|.
bool parse_interact_source(const std::string& src,
                           ScriptAst* out,
                           std::string* err);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_WIRE_PARSE_H_
