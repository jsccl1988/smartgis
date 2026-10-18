// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_FRONT_AST_H_
#define IL_RUNTIME_CODEGEN_FRONT_AST_H_

#include "content/browser/glang.runtime/language/front/diagnostic.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace content {
namespace py {

enum class ExprKind { kName, kConst, kAttribute, kCall, kUnsupported };
enum class ConstKind { kNone, kBool, kInt, kStr };
enum class StmtKind { kImport, kFunc, kAssign, kExpr, kUnsupported };

// Subset expression. String constants store the decoded literal value in text.
// Anything the grammar accepts outside the subset is kUnsupported.
struct Expr {
  ExprKind kind = ExprKind::kUnsupported;
  Loc loc;
  ConstKind const_kind = ConstKind::kNone;
  bool bool_value = false;
  int64_t int_value = 0;
  std::string text;  // name, string literal, or unsupported snippet
  std::string attr;  // attribute name when kind==kAttribute or the method on a call receiver
  std::vector<Expr> args;                          // positional
  std::vector<std::pair<std::string, Expr>> named;
  std::unique_ptr<Expr> receiver;  // for attribute/call; null otherwise

  Expr() = default;
  Expr(Expr&&) noexcept = default;
  Expr& operator=(Expr&&) noexcept = default;
  Expr(const Expr&) = delete;
  Expr& operator=(const Expr&) = delete;
  ~Expr();
};

// Subset statement. if/for/class and other unmapped forms stay kUnsupported
// with loc set so semantic analysis can reject them.
struct Stmt {
  StmtKind kind = StmtKind::kUnsupported;
  Loc loc;
  std::string name;            // import module, func name, assign target
  std::unique_ptr<Expr> expr;  // assign rhs or expr stmt
  std::vector<Stmt> body;      // func body

  Stmt() = default;
  Stmt(Stmt&&) noexcept = default;
  Stmt& operator=(Stmt&&) noexcept = default;
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;
  ~Stmt();
};

// One Python module. Comments and a leading string literal are omitted.
struct File {
  std::vector<Stmt> body;
};

}  // namespace py
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_FRONT_AST_H_
