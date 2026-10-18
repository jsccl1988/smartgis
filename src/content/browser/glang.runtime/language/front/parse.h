// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_FRONT_PARSE_H_
#define IL_RUNTIME_CODEGEN_FRONT_PARSE_H_

#include "content/browser/glang.runtime/language/front/ast.h"

#include <string>
#include <string_view>
#include <vector>

namespace content {
namespace py {

// Bail on the first syntax error. On failure `out` may be empty and `diags`
// has one entry. Does not include generated ANTLR headers.
bool parse_python(std::string_view source, File* out, std::vector<Diagnostic>* diags);

namespace detail {

// Clear `out`, keep a single diagnostic, and log it. Always returns false.
bool fail_parse(File* out,
                std::vector<Diagnostic>* diags,
                Loc loc,
                std::string message);

}  // namespace detail
}  // namespace py
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_FRONT_PARSE_H_
