// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_ARGS_H_
#define IL_RUNTIME_CODEGEN_LOWER_ARGS_H_

#include <string>
#include <string_view>
#include <vector>

#include "app/views/il.runtime/frontend/ast.h"

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

bool lookup_var(const VarMap* vars, const std::string& key, std::string* out);

// If |raw| is `$name`, look it up; otherwise return |raw|.
std::string resolve_ident(const std::string& raw, const VarMap* vars);

bool bind_as(VarMap* vars, const std::string& as, const std::string& path);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_ARGS_H_
