// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/lower.h"

#include <cstdio>

#include "app/views/il.runtime/backend/eval_host.h"
#include "app/views/il.runtime/backend/lower_document.h"
#include "app/views/il.runtime/backend/lower_expect.h"
#include "app/views/il.runtime/backend/horizon.h"
#include "app/views/il.runtime/backend/input.h"
#include "app/views/il.runtime/backend/plugin.h"

namespace app {
namespace detail {

std::optional<Action> lower_call(const CallStmt& call, VarMap* vars) {
  if (std::optional<Action> action = try_lower_horizon_call(call, vars)) {
    return action;
  }
  if (std::optional<Action> action = try_lower_document_call(call, vars)) {
    return action;
  }
  if (std::optional<Action> action = try_lower_plugin_call(call, vars)) {
    return action;
  }
  if (std::optional<Action> action = try_lower_expect_call(call, vars)) {
    return action;
  }
  if (std::optional<Action> action = try_lower_input_call(call, vars)) {
    return action;
  }
  return std::nullopt;
}

bool lower_stmt(const Stmt& stmt, VarMap* vars, Code* out) {
  if (!out) {
    return false;
  }
  if (const CallStmt* call = as_call(stmt)) {
    if (!driver_ok(call->drivers)) {
      *out = Code::nop();
      return true;
    }
    if (std::optional<Action> action = lower_call(*call, vars)) {
      *out = Code::run_named(call->name, std::move(*action));
      return true;
    }
    std::fprintf(stderr, "interact-dsl: skip unknown call '%s'\n",
                 call->name.c_str());
    *out = Code::nop();
    return true;
  }
  const BlockStmt* block = as_block(stmt);
  if (!block) {
    return false;
  }
  if (!driver_ok(block->drivers)) {
    *out = Code::nop();
    return true;
  }
  Code lowered;
  if (block->kind == BlockStmt::Kind::kSeq) {
    lowered.kind = Code::Kind::kSeq;
    lowered.name = "seq";
  } else if (block->kind == BlockStmt::Kind::kRepeat) {
    lowered.kind = Code::Kind::kRepeat;
    lowered.name = "repeat";
    lowered.repeat_count = block->repeat_count;
  } else if (block->kind == BlockStmt::Kind::kChord) {
    lowered.kind = Code::Kind::kChord;
    lowered.name = "chord";
    lowered.chord_mods = block->chord_mods;
  } else {
    return false;
  }
  lowered.body.reserve(block->body.size());
  for (const Stmt& s : block->body) {
    Code child;
    if (!lower_stmt(s, vars, &child)) {
      return false;
    }
    lowered.body.push_back(std::move(child));
  }
  *out = std::move(lowered);
  return true;
}

Code lower_script(const ScriptAst& ast, VarMap* vars) {
  Code root;
  root.kind = Code::Kind::kSeq;
  root.name = ast.name.empty() ? "script" : ast.name;
  root.body.reserve(ast.stmts.size());
  for (const Stmt& s : ast.stmts) {
    Code child;
    if (!lower_stmt(s, vars, &child)) {
      child = Code::nop();
    }
    root.body.push_back(std::move(child));
  }
  return root;
}

}  // namespace detail
}  // namespace app
