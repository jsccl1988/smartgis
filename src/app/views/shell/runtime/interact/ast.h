// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_RUNTIME_INTERACT_AST_H_
#define APP_VIEWS_SHELL_RUNTIME_INTERACT_AST_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace app {
namespace detail {

// Pixel point in client coordinates (Interact.g4 point / pointList).
struct Point {
  int x = 0;
  int y = 0;
};

// Parsed literal / ident / point value for a call argument.
struct Value {
  enum class Kind { kInt, kString, kIdent, kPoint, kPointList };
  Kind kind = Kind::kInt;
  int ival = 0;
  std::string sval;
  Point point{};
  std::vector<Point> points;
};

// Named or positional argument attached to a CallStmt.
struct Arg {
  std::string name;  // empty => positional
  Value value;
};

enum class DriverFilter { kAny, kInproc, kOs };

struct Stmt;

// Leaf command: name + args + optional @drivers / @inject.
struct CallStmt {
  std::string name;
  std::vector<Arg> args;
  DriverFilter drivers = DriverFilter::kAny;
  std::string inject;  // empty / postmessage / sendinput
};

// Compound seq / repeat / chord block with nested body.
struct BlockStmt {
  enum class Kind { kSeq, kRepeat, kChord };
  Kind kind = Kind::kSeq;
  int repeat_count = 1;
  std::vector<std::string> chord_mods;
  DriverFilter drivers = DriverFilter::kAny;
  std::vector<Stmt> body;
};

// Statement node: either a call or a heap-backed block.
struct Stmt {
  enum class Kind { kCall, kBlock };
  Kind kind = Kind::kCall;
  CallStmt call;
  // Heap-allocated to avoid incomplete-type / recursive layout UB.
  std::unique_ptr<BlockStmt> block;
};

// Top-level Interact script AST after ANTLR walk.
struct ScriptAst {
  std::string name;
  std::vector<Stmt> stmts;
};

using VarMap = std::unordered_map<std::string, std::string>;

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_RUNTIME_INTERACT_AST_H_
