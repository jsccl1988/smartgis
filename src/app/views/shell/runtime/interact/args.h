// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_INTERACT_ARGS_H_
#define APP_VIEWS_SHELL_RUNTIME_INTERACT_ARGS_H_

#include <string>
#include <vector>

#include "app/views/shell/runtime/interact/ast.h"

namespace app {
namespace detail {

int arg_int(const CallStmt& c, int positional, const char* named, int def);

std::string arg_ident(const CallStmt& c,
                      int positional,
                      const char* named,
                      const char* def);

std::vector<Point> arg_points(const CallStmt& c);

std::string json_escape_path(const std::string& s);

// Replace $name (IDENT) with json-escaped var values for processing args.
std::string expand_vars(const std::string& in, const VarMap& vars);

bool bind_as(VarMap* vars, const CallStmt& c, const std::string& path);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_INTERACT_ARGS_H_
