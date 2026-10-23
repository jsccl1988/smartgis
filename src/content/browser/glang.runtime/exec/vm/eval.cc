// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/vm/eval.h"

#include "content/browser/glang.runtime/host/tile/verbs.h"
#include "content/browser/glang.runtime/exec/vm/verb.h"
#include "content/browser/glang.runtime/language/lower/lower.h"
#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/glang.runtime/language/sema/check.h"
#include "base/core/log.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace content {
namespace code {
namespace {

std::string_view trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

bool parse_i32(std::string_view text, std::int32_t* out) {
  text = trim(text);
  if (text.empty() || !out) {
    return false;
  }
  if (text.front() == '+') {
    text.remove_prefix(1);
  }
  std::int32_t value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) {
    return false;
  }
  *out = value;
  return true;
}

bool is_bool_word(std::string_view text, bool* value) {
  text = trim(text);
  if (text == "1" || text == "True" || text == "true") {
    *value = true;
    return true;
  }
  if (text == "0" || text == "False" || text == "false") {
    *value = false;
    return true;
  }
  return false;
}

const Param* param_at(const OpDesc* desc, int positional) {
  if (!desc || !desc->params || positional < 0) {
    return nullptr;
  }
  for (int i = 0; i < desc->param_count; ++i) {
    if (desc->params[i].positional == positional) {
      return &desc->params[i];
    }
  }
  return nullptr;
}

const Param* param_named(const OpDesc* desc, std::string_view name) {
  if (!desc || !desc->params) {
    return nullptr;
  }
  for (int i = 0; i < desc->param_count; ++i) {
    if (desc->params[i].name && name == desc->params[i].name) {
      return &desc->params[i];
    }
  }
  return nullptr;
}

content::py::Expr literal_from_text(std::string_view text, const Param* param) {
  content::py::Expr expr;
  expr.kind = content::py::ExprKind::kConst;
  text = trim(text);
  const content::code::ArgKind kind = param ? param->kind : content::code::ArgKind::kStr;
  if (kind == content::code::ArgKind::kI32 || kind == content::code::ArgKind::kBool ||
      kind == content::code::ArgKind::kVk) {
    bool flag = false;
    if (kind != content::code::ArgKind::kVk && is_bool_word(text, &flag) &&
        (kind == content::code::ArgKind::kBool || text == "True" || text == "true" ||
         text == "False" || text == "false")) {
      expr.const_kind = content::py::ConstKind::kBool;
      expr.bool_value = flag;
      return expr;
    }
    std::int32_t number = 0;
    if (parse_i32(text, &number) &&
        (kind == content::code::ArgKind::kI32 || kind == content::code::ArgKind::kVk ||
         (kind == content::code::ArgKind::kBool && (number == 0 || number == 1)))) {
      expr.const_kind = content::py::ConstKind::kInt;
      expr.int_value = number;
      return expr;
    }
  }
  expr.const_kind = content::py::ConstKind::kStr;
  expr.text = std::string(text);
  return expr;
}

content::py::Expr call_expr(const ScriptCall& call, const OpDesc* desc) {
  content::py::Expr expr;
  expr.kind = content::py::ExprKind::kCall;
  expr.attr = call.name;
  content::py::Expr receiver;
  receiver.kind = content::py::ExprKind::kName;
  receiver.text = "sg";
  expr.receiver = std::make_unique<content::py::Expr>(std::move(receiver));
  for (std::size_t i = 0; i < call.positional.size(); ++i) {
    expr.args.push_back(
        literal_from_text(call.positional[i], param_at(desc, static_cast<int>(i))));
  }
  for (const auto& named : call.named) {
    const Param* param = nullptr;
    if (named.first != "as" && named.first != "as_") {
      param = param_named(desc, named.first);
    }
    expr.named.emplace_back(named.first, literal_from_text(named.second, param));
  }
  return expr;
}

bool run_module(content::CapabilityHost& host,
                const Module& module,
                SlotTable* seeded,
                std::string* error) {
  ensure_ops_registered();
  SlotTable local;
  SlotTable* slots = seeded;
  if (!slots) {
    local.values.assign(module.slot_names.size(), std::string());
    slots = &local;
  } else if (slots->values.size() < module.slot_names.size()) {
    slots->values.resize(module.slot_names.size());
  }
  for (const Inst& inst : module.insts) {
    InvokeKind kind = InvokeKind::kRan;
    std::string detail;
    if (invoke_inst(host, module, inst, *slots, &kind, &detail)) {
      continue;
    }
    if (kind == InvokeKind::kMissing) {
      const std::string text = "missing descriptor line " + std::to_string(inst.loc.line);
      if (error) {
        *error = text;
      }
      LOGGING(LOG_ERROR, "eval %s", text.c_str());
      return false;
    }
    const OpDesc* desc = find_op(inst.op);
    const char* name = desc && desc->name && desc->name[0] ? desc->name : "op";
    std::string text = std::string(name) + " line " + std::to_string(inst.loc.line);
    if (!detail.empty()) {
      text += ": ";
      text += detail;
    }
    if (error) {
      *error = std::move(text);
    }
    LOGGING(LOG_ERROR, "eval %s line %u: %s", name, inst.loc.line,
            detail.empty() ? "failed" : detail.c_str());
    return false;
  }
  return true;
}

}  // namespace

bool eval_module(content::CapabilityHost& host,
                 const Module& module,
                 std::string* error) {
  return run_module(host, module, nullptr, error);
}

bool eval_script_call(content::CapabilityHost& host,
                      const ScriptCall& call,
                      std::unordered_map<std::string, std::string>* vars,
                      std::string* error,
                      std::string* result) {
  std::string tile_error;
  std::string tile_result;
  if (content::tile::try_script(call, &tile_result, &tile_error)) {
    if (!tile_error.empty()) {
      if (error) {
        *error = std::move(tile_error);
      }
      return false;
    }
    if (result) {
      *result = std::move(tile_result);
    }
    return true;
  }
  ensure_ops_registered();
  const OpDesc* desc = content::py::lookup_method(call.name);
  if (!desc) {
    const std::string text = "unknown verb '" + call.name + "'";
    if (error) {
      *error = text;
    }
    LOGGING(LOG_ERROR, "script: %s", text.c_str());
    return false;
  }

  content::py::BoundProgram program;
  if (vars) {
    std::vector<std::string> keys;
    keys.reserve(vars->size());
    for (const auto& entry : *vars) {
      keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    program.slot_names = std::move(keys);
  }
  std::vector<content::py::Diagnostic> diags;
  const content::py::Expr expr = call_expr(call, desc);
  if (!content::py::bind_method(expr, call.name, content::py::Loc{}, &program, &diags,
                            true)) {
    const std::string text = diags.empty() ? "script call rejected" : diags.front().message;
    if (error) {
      *error = text;
    }
    return false;
  }

  Module module;
  diags.clear();
  if (!lower_program(program, &module, &diags)) {
    const std::string text = diags.empty() ? "lower failed" : diags.front().message;
    if (error) {
      *error = text;
    }
    return false;
  }

  SlotTable slots;
  slots.values.resize(module.slot_names.size());
  if (vars) {
    for (std::size_t i = 0; i < module.slot_names.size(); ++i) {
      const auto it = vars->find(module.slot_names[i]);
      if (it != vars->end()) {
        slots.values[i] = it->second;
      }
    }
  }
  if (!run_module(host, module, &slots, error)) {
    return false;
  }
  if (desc->produces_slot && !module.insts.empty()) {
    const Inst& inst = module.insts.back();
    if (inst.result_slot >= 0 &&
        static_cast<std::size_t>(inst.result_slot) < slots.values.size() &&
        static_cast<std::size_t>(inst.result_slot) < module.slot_names.size()) {
      const std::string& value =
          slots.values[static_cast<std::size_t>(inst.result_slot)];
      if (vars) {
        (*vars)[module.slot_names[static_cast<std::size_t>(inst.result_slot)]] =
            value;
      }
      if (result) {
        *result = value;
      }
    }
  }
  return true;
}

}  // namespace code
}  // namespace content
