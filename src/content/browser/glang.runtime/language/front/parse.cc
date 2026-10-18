// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/language/front/parse.h"

#include "base/core/log.h"

#include <string>
#include <utility>

namespace content {
namespace py {
namespace detail {

bool fail_parse(File* out,
                std::vector<Diagnostic>* diags,
                Loc loc,
                std::string message) {
  if (out != nullptr) {
    out->body.clear();
  }
  if (loc.line == 0) {
    loc.line = 1;
  }
  if (loc.column == 0) {
    loc.column = 1;
  }
  LOGGING(LOG_ERROR, "python parse %u:%u: %s", loc.line, loc.column, message.c_str());
  if (diags != nullptr) {
    diags->clear();
    Diagnostic diag;
    diag.loc = loc;
    diag.message = std::move(message);
    diags->push_back(std::move(diag));
  }
  return false;
}

}  // namespace detail

Expr::~Expr() = default;
Stmt::~Stmt() = default;

}  // namespace py
}  // namespace content
