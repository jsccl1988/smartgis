// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_WIRE_AST_H_
#define APP_VIEWS_RUNTIME_INTERACT_WIRE_AST_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "base/tuple/tuple.h"

namespace app {
namespace detail {

using Point = base::named_tuple<base::named<"x", int>, base::named<"y", int>>;

struct IntTag {};
struct StringTag {};
struct IdentTag {};
struct PointTag {};
struct PointListTag {};

// Interact literal: one active alternative, tagged (not parallel fields).
using Value = std::variant<base::tagged<IntTag, int>,
                           base::tagged<StringTag, std::string>,
                           base::tagged<IdentTag, std::string>,
                           base::tagged<PointTag, Point>,
                           base::tagged<PointListTag, std::vector<Point>>>;

inline Value make_int(int v) {
  return base::tagged<IntTag, int>{v};
}
inline Value make_string(std::string v) {
  return base::tagged<StringTag, std::string>{std::move(v)};
}
inline Value make_ident(std::string v) {
  return base::tagged<IdentTag, std::string>{std::move(v)};
}
inline Value make_point(Point v) {
  return base::tagged<PointTag, Point>{std::move(v)};
}
inline Value make_point_list(std::vector<Point> v) {
  return base::tagged<PointListTag, std::vector<Point>>{std::move(v)};
}

inline bool is_int(const Value& v) {
  return std::holds_alternative<base::tagged<IntTag, int>>(v);
}
inline bool is_text(const Value& v) {
  return std::holds_alternative<base::tagged<StringTag, std::string>>(v) ||
         std::holds_alternative<base::tagged<IdentTag, std::string>>(v);
}
inline bool is_point(const Value& v) {
  return std::holds_alternative<base::tagged<PointTag, Point>>(v);
}
inline bool is_point_list(const Value& v) {
  return std::holds_alternative<base::tagged<PointListTag, std::vector<Point>>>(
      v);
}

inline int as_int(const Value& v, int def = 0) {
  if (const auto* p = std::get_if<base::tagged<IntTag, int>>(&v)) {
    return p->value;
  }
  return def;
}

inline const std::string* as_text(const Value& v) {
  if (const auto* p = std::get_if<base::tagged<StringTag, std::string>>(&v)) {
    return &p->value;
  }
  if (const auto* p = std::get_if<base::tagged<IdentTag, std::string>>(&v)) {
    return &p->value;
  }
  return nullptr;
}

inline const Point* as_point(const Value& v) {
  if (const auto* p = std::get_if<base::tagged<PointTag, Point>>(&v)) {
    return &p->value;
  }
  return nullptr;
}

inline const std::vector<Point>* as_point_list(const Value& v) {
  if (const auto* p =
          std::get_if<base::tagged<PointListTag, std::vector<Point>>>(&v)) {
    return &p->value;
  }
  return nullptr;
}

using Arg = base::named_tuple<base::named<"name", std::string>,
                              base::named<"value", Value>>;

enum class DriverFilter { kAny, kInproc, kOs };

struct Stmt;

// Leaf command: name + args + optional @drivers / @inject.
struct CallStmt {
  std::string name;
  std::vector<Arg> args;
  DriverFilter drivers = DriverFilter::kAny;
  std::string inject;
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

// Statement node: call or heap-backed block (no unused sibling fields).
struct Stmt {
  std::variant<CallStmt, std::unique_ptr<BlockStmt>> node;
};

inline bool is_call(const Stmt& s) {
  return std::holds_alternative<CallStmt>(s.node);
}
inline const CallStmt* as_call(const Stmt& s) {
  return std::get_if<CallStmt>(&s.node);
}
inline CallStmt* as_call(Stmt& s) {
  return std::get_if<CallStmt>(&s.node);
}
inline const BlockStmt* as_block(const Stmt& s) {
  if (const auto* p = std::get_if<std::unique_ptr<BlockStmt>>(&s.node)) {
    return p->get();
  }
  return nullptr;
}
inline BlockStmt* as_block(Stmt& s) {
  if (auto* p = std::get_if<std::unique_ptr<BlockStmt>>(&s.node)) {
    return p->get();
  }
  return nullptr;
}

// Top-level Interact script AST after ANTLR walk.
struct ScriptAst {
  std::string name;
  std::vector<Stmt> stmts;
};

using VarMap = std::unordered_map<std::string, std::string>;

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_WIRE_AST_H_
