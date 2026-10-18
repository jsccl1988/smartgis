// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/session/plugin_fn.h"

#include "base/core/log.h"
#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/glang.runtime/language/sema/check.h"

#include <mutex>
#include <utility>
#include <vector>

namespace content {
namespace py {
namespace {

struct Table {
  std::mutex mu;
  std::vector<RegisteredFn> entries;
  bool builtins = false;
};

Table& table() {
  static Table instance;
  return instance;
}

void add_or_replace(Table* reg,
                    std::string_view name,
                    const code::Param* params,
                    int param_count,
                    code::EvalFn eval) {
  for (RegisteredFn& entry : reg->entries) {
    if (entry.name == name) {
      entry.params = params;
      entry.param_count = param_count;
      entry.eval = eval;
      return;
    }
  }
  RegisteredFn created;
  created.name = std::string(name);
  created.params = params;
  created.param_count = param_count;
  created.eval = eval;
  reg->entries.push_back(std::move(created));
}

void copy_desc(Table* reg, const code::OpDesc* desc) {
  if (!reg || !desc || !desc->name || !desc->name[0] || !desc->eval) {
    return;
  }
  add_or_replace(reg, desc->name, desc->params, desc->param_count, desc->eval);
  if (!desc->aliases) {
    return;
  }
  for (const char* const* alias = desc->aliases; *alias; ++alias) {
    if (*alias && (*alias)[0]) {
      add_or_replace(reg, *alias, desc->params, desc->param_count, desc->eval);
    }
  }
}

void ensure_builtins(Table* reg) {
  if (!reg || reg->builtins) {
    return;
  }
  reg->builtins = true;
  code::ensure_ops_registered();
  for (int i = 0; i < static_cast<int>(code::Op::kCount); ++i) {
    copy_desc(reg, code::find_op(static_cast<code::Op>(i)));
  }
}

const RegisteredFn* find_locked(Table* reg, std::string_view name) {
  for (const RegisteredFn& entry : reg->entries) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace

bool register_fn(std::string_view name,
                 const code::Param* params,
                 int param_count,
                 code::EvalFn eval) {
  if (name.empty() || !eval) {
    return false;
  }
  Table& reg = table();
  std::lock_guard<std::mutex> lock(reg.mu);
  ensure_builtins(&reg);
  add_or_replace(&reg, name, params, param_count, eval);
  return true;
}

void unregister_fn(std::string_view name) {
  if (name.empty()) {
    return;
  }
  Table& reg = table();
  std::lock_guard<std::mutex> lock(reg.mu);
  ensure_builtins(&reg);
  std::vector<RegisteredFn> kept;
  kept.reserve(reg.entries.size());
  for (RegisteredFn& entry : reg.entries) {
    if (entry.name != name) {
      kept.push_back(std::move(entry));
    }
  }
  reg.entries = std::move(kept);
}

std::optional<RegisteredFn> find_fn(std::string_view name) {
  if (name.empty()) {
    return std::nullopt;
  }
  Table& reg = table();
  std::lock_guard<std::mutex> lock(reg.mu);
  ensure_builtins(&reg);
  const RegisteredFn* found = find_locked(&reg, name);
  if (!found) {
    return std::nullopt;
  }
  return *found;
}

bool call_registered(const code::ScriptCall& call,
                     std::string* error,
                     std::string* result) {
  // rclick / dblclick / key_down / key_up / path are sema sugar, not Op names.
  // eval_script_call applies the rewrite via lookup_method.
  if (!find_fn(call.name) && lookup_method(call.name) == nullptr) {
    const std::string text = "unknown verb '" + call.name + "'";
    if (error) {
      *error = text;
    }
    LOGGING(LOG_ERROR, "plugin_fn: %s", text.c_str());
    return false;
  }
  code::ScriptHandler* handler = code::script_handler();
  if (!handler) {
    if (error) {
      *error = "script host not bound";
    }
    return false;
  }
  return handler->call(call, error, result);
}

}  // namespace py
}  // namespace content
