// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_POLICY_ARGS_H_
#define APP_VIEWS_RUNTIME_INTERACT_POLICY_ARGS_H_

#include <string>
#include <string_view>
#include <vector>

#include "app/views/runtime/interact/wire/ast.h"

namespace app {
namespace detail {

int arg_int(const CallStmt& c,
            int positional,
            std::string_view named,
            int def);

std::string arg_ident(const CallStmt& c,
                      int positional,
                      std::string_view named,
                      std::string_view def);

std::vector<Point> arg_points(const CallStmt& c);

std::string json_escape_path(const std::string& s);

// Replace $name (IDENT) with json-escaped var values for processing args.
std::string expand_vars(const std::string& in, const VarMap& vars);

bool bind_as(VarMap* vars, const CallStmt& c, const std::string& path);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_POLICY_ARGS_H_
