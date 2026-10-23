// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/vm/verb.h"

#include "content/browser/glang.runtime/exec/vm/exec.h"
#include "content/browser/glang.runtime/exec/vm/language.h"
#include "content/browser/glang.runtime/exec/op/host_verb.h"
#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/glang.runtime/exec/py/plugin_fn.h"

#include <charconv>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace content {
namespace code {
namespace {

struct HostVerbTable {
  static constexpr int kMax = 24;
  const OpDesc* items[kMax]{};
  int count = 0;
  void (*install)() = nullptr;
  bool installed = false;
};

HostVerbTable& host_verb_table() {
  static HostVerbTable table;
  return table;
}

void install_host_verbs() {
  HostVerbTable& table = host_verb_table();
  if (table.installed || !table.install) {
    return;
  }
  table.installed = true;
  table.install();
}

struct NameRef {
  const char* name;
  const OpDesc* desc;
};

struct Registry {
  static constexpr int kMaxNames = 160;
  const OpDesc* by_op[static_cast<std::size_t>(Op::kCount)]{};
  NameRef names[kMaxNames]{};
  int name_count = 0;
};

void add_name(Registry* reg, const char* name, const OpDesc* desc) {
  if (!reg || !name || !name[0] || !desc) {
    return;
  }
  for (int i = 0; i < reg->name_count; ++i) {
    if (std::string_view(reg->names[i].name) == name) {
      LOGGING(LOG_ERROR, "code: duplicate op name %s", name);
      return;
    }
  }
  if (reg->name_count >= Registry::kMaxNames) {
    LOGGING(LOG_ERROR, "code: op name table full at %s", name);
    return;
  }
  reg->names[reg->name_count++] = NameRef{name, desc};
}

Registry build_registry() {
  Registry reg;
  static_assert(static_cast<int>(Op::kCount) == 105, "opcode table size");
  install_host_verbs();
  fill_document_ops(reg.by_op);
  fill_view_ops(reg.by_op);
  fill_plugin_ops(reg.by_op);
  fill_horizon_ops(reg.by_op);
  fill_gesture_ops(reg.by_op);
  fill_buffer_op(reg.by_op);
  fill_gis_ops(reg.by_op);
  const HostVerbTable& host_verbs = host_verb_table();
  for (int i = 0; i < host_verbs.count; ++i) {
    const OpDesc* desc = host_verbs.items[i];
    if (!desc) {
      continue;
    }
    const auto index = static_cast<std::size_t>(desc->op);
    if (index >= static_cast<std::size_t>(Op::kCount)) {
      LOGGING(LOG_ERROR, "code: host verb %s has no slot",
              desc->name ? desc->name : "op");
      continue;
    }
    if (reg.by_op[index]) {
      LOGGING(LOG_ERROR, "code: host verb %s collides with a language op",
              desc->name ? desc->name : "op");
      continue;
    }
    reg.by_op[index] = desc;
  }
  for (std::size_t i = 0; i < static_cast<std::size_t>(Op::kCount); ++i) {
    const OpDesc* desc = reg.by_op[i];
    if (!desc) {
      if (is_host_verb_slot(static_cast<Op>(i))) {
        continue;
      }
      LOGGING(LOG_ERROR, "code: missing descriptor for op %u",
              static_cast<unsigned>(i));
      continue;
    }
    add_name(&reg, desc->name, desc);
    if (!desc->aliases) {
      continue;
    }
    for (const char* const* alias = desc->aliases; *alias; ++alias) {
      add_name(&reg, *alias, desc);
    }
  }
  return reg;
}

const Registry& registry() {
  static const Registry reg = build_registry();
  return reg;
}

// Dynamic names, exec-extension fills, and language lower hooks.
// Builtin descriptors stay in Registry and are not copied here.
struct DynVerb {
  std::string name;
  const Param* params = nullptr;
  int param_count = 0;
  EvalFn eval = nullptr;
  bool hidden = false;
};

struct Overlay {
  std::mutex mu;
  std::vector<DynVerb> dynamic;
  std::vector<ExecExtension> execs;
  std::vector<LanguageExtension> languages;
};

Overlay& overlay() {
  static Overlay table;
  return table;
}

DynVerb* find_dynamic_locked(Overlay* table, std::string_view name) {
  for (DynVerb& entry : table->dynamic) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

EvalFn find_exec_locked(const Overlay& table, Op op) {
  for (const ExecExtension& slot : table.execs) {
    if (slot.op == op) {
      return slot.eval;
    }
  }
  return nullptr;
}

bool is_gis_op(Op op) {
  return static_cast<int>(op) >= static_cast<int>(Op::kBuffer) && op < Op::kCount;
}

int32_t parse_epsg(std::string_view text) {
  int32_t value = 0;
  if (text.empty()) {
    return 0;
  }
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || value <= 0) {
    return 0;
  }
  return value;
}

}  // namespace

const OpDesc* find_op(std::string_view script_name) {
  const Registry& reg = registry();
  for (int i = 0; i < reg.name_count; ++i) {
    if (script_name == reg.names[i].name) {
      return reg.names[i].desc;
    }
  }
  return nullptr;
}

const OpDesc* find_op(Op op) {
  const Registry& reg = registry();
  const auto index = static_cast<std::size_t>(op);
  if (index >= static_cast<std::size_t>(Op::kCount)) {
    return nullptr;
  }
  return reg.by_op[index];
}

void ensure_ops_registered() {
  (void)registry();
}

void set_host_verb_installer(void (*install)()) {
  host_verb_table().install = install;
}

void register_host_verb(const OpDesc* desc) {
  if (!desc || !desc->name || !desc->name[0] || !desc->eval) {
    return;
  }
  HostVerbTable& table = host_verb_table();
  for (int i = 0; i < table.count; ++i) {
    if (table.items[i] && table.items[i]->op == desc->op) {
      table.items[i] = desc;
      return;
    }
  }
  if (table.count >= HostVerbTable::kMax) {
    LOGGING(LOG_ERROR, "code: host verb table full at %s", desc->name);
    return;
  }
  table.items[table.count++] = desc;
}

bool invoke_inst(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 InvokeKind* kind,
                 std::string* error) {
  if (kind) {
    *kind = InvokeKind::kRan;
  }
  const OpDesc* desc = find_op(inst.op);
  const EvalFn eval = detail::resolve_eval(desc);
  if (!desc || !eval) {
    if (kind) {
      *kind = InvokeKind::kMissing;
    }
    return false;
  }
  return eval(host, module, inst, slots, error);
}

namespace detail {

EvalFn resolve_eval(const OpDesc* desc) {
  if (!desc) {
    return nullptr;
  }
  if (desc->eval) {
    return desc->eval;
  }
  Overlay& table = overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  return find_exec_locked(table, desc->op);
}

}  // namespace detail

void register_language_extension(const LanguageExtension& ext) {
  if (!ext.name || !ext.name[0] || !ext.lower_call) {
    return;
  }
  Overlay& table = overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  for (LanguageExtension& slot : table.languages) {
    if (slot.name && std::string_view(slot.name) == ext.name) {
      slot = ext;
      return;
    }
  }
  table.languages.push_back(ext);
}

void register_exec_extension(const ExecExtension& ext) {
  if (!ext.eval) {
    return;
  }
  Overlay& table = overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  for (ExecExtension& slot : table.execs) {
    if (slot.op == ext.op) {
      slot = ext;
      return;
    }
  }
  table.execs.push_back(ext);
}

EvalFn find_exec_extension(Op op) {
  Overlay& table = overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  return find_exec_locked(table, op);
}

bool lower_gis_call(const CallLowering& call, Module* extra, std::string* error) {
  const OpDesc* desc = find_op(call.name);
  if (!desc || !is_gis_op(desc->op)) {
    return false;
  }
  if (!extra) {
    if (error) {
      *error = "no module";
    }
    return false;
  }
  int32_t crs = 0;
  std::string dst_epsg;
  for (int i = 0; i < call.named_count; ++i) {
    if (!call.named) {
      break;
    }
    const NamedArg& named = call.named[i];
    if (named.key == "crs" || named.key == "epsg") {
      crs = parse_epsg(named.value);
    } else if (named.key == "dst_epsg") {
      dst_epsg = std::string(named.value);
    }
  }
  int required = 0;
  if (desc->params) {
    for (int i = 0; i < desc->param_count; ++i) {
      if (desc->params[i].required && desc->params[i].positional >= 0) {
        ++required;
      }
    }
  }
  const int given = call.positional_count > 0 ? call.positional_count : 0;
  if (given < required && dst_epsg.empty()) {
    if (error) {
      *error = std::string(desc->name ? desc->name : "gis") + ": missing operand";
    }
    return false;
  }

  Inst inst;
  inst.op = desc->op;
  inst.loc = call.loc;
  inst.operand_begin = static_cast<uint32_t>(extra->operands.size());
  inst.result_slot = -1;
  const std::string* positional = call.positional;
  for (int i = 0; i < given; ++i) {
    Operand operand;
    operand.kind = ArgKind::kStr;
    operand.crs_epsg = crs;
    operand.index = static_cast<uint32_t>(extra->strings.size());
    extra->strings.push_back(positional ? positional[i] : std::string());
    extra->operands.push_back(operand);
  }
  if (!dst_epsg.empty() && given < 2) {
    Operand operand;
    operand.kind = ArgKind::kStr;
    operand.crs_epsg = parse_epsg(dst_epsg);
    operand.index = static_cast<uint32_t>(extra->strings.size());
    extra->strings.push_back(dst_epsg);
    extra->operands.push_back(operand);
  }
  inst.operand_count = static_cast<uint16_t>(extra->operands.size() - inst.operand_begin);
  extra->insts.push_back(inst);
  return true;
}

bool lower_extended_call(const CallLowering& call, Module* extra, std::string* error) {
  std::string gis_error;
  if (lower_gis_call(call, extra, &gis_error)) {
    return true;
  }
  if (!gis_error.empty()) {
    if (error) {
      *error = std::move(gis_error);
    }
    return false;
  }
  std::vector<LanguageExtension> languages;
  {
    Overlay& table = overlay();
    std::lock_guard<std::mutex> lock(table.mu);
    languages = table.languages;
  }
  for (const LanguageExtension& slot : languages) {
    if (!slot.lower_call) {
      continue;
    }
    std::string local;
    if (slot.lower_call(call, extra, &local)) {
      return true;
    }
    if (!local.empty()) {
      if (error) {
        *error = std::move(local);
      }
      return false;
    }
  }
  if (error) {
    *error = "no language extension";
  }
  return false;
}

bool eval_gis_call(content::CapabilityHost& host,
                   std::string_view op_name,
                   const GisArg* args,
                   int arg_count,
                   std::string* result,
                   std::string* error) {
  ensure_ops_registered();
  const OpDesc* desc = find_op(op_name);
  if (!desc) {
    if (error) {
      *error = "unknown op " + std::string(op_name);
    }
    LOGGING(LOG_ERROR, "gis call: unknown op %.*s",
            static_cast<int>(op_name.size()), op_name.data());
    return false;
  }
  if (arg_count < 0) {
    arg_count = 0;
  }

  Module module;
  module.slot_names.emplace_back("out");
  module.operands.reserve(static_cast<std::size_t>(arg_count));
  for (int i = 0; i < arg_count; ++i) {
    const GisArg& arg = args ? args[i] : GisArg{};
    Operand operand;
    operand.kind = arg.kind;
    operand.i32 = arg.i32;
    operand.crs_epsg = arg.crs_epsg;
    if (operand.kind != ArgKind::kI32 && operand.kind != ArgKind::kBool) {
      operand.index = static_cast<uint32_t>(module.strings.size());
      module.strings.push_back(arg.text);
    }
    module.operands.push_back(operand);
  }

  Inst inst;
  inst.op = desc->op;
  inst.operand_begin = 0;
  inst.operand_count = static_cast<uint16_t>(arg_count);
  inst.result_slot = 0;
  module.insts.push_back(inst);

  SlotTable slots;
  slots.values.emplace_back();
  InvokeKind kind = InvokeKind::kRan;
  if (!invoke_inst(host, module, module.insts.front(), slots, &kind, error)) {
    if (kind == InvokeKind::kMissing) {
      if (error) {
        *error = "no eval";
      }
      LOGGING(LOG_ERROR, "gis call: no eval for %s", desc->name ? desc->name : "op");
    }
    return false;
  }
  if (result && !slots.values.empty()) {
    *result = slots.values.front();
  }
  return true;
}

}  // namespace code

namespace py {

bool register_fn(std::string_view name,
                 const code::Param* params,
                 int param_count,
                 code::EvalFn eval) {
  if (name.empty() || !eval) {
    return false;
  }
  code::Overlay& table = code::overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  if (code::DynVerb* existing = code::find_dynamic_locked(&table, name)) {
    existing->params = params;
    existing->param_count = param_count;
    existing->eval = eval;
    existing->hidden = false;
    return true;
  }
  code::DynVerb created;
  created.name = std::string(name);
  created.params = params;
  created.param_count = param_count;
  created.eval = eval;
  table.dynamic.push_back(std::move(created));
  return true;
}

void unregister_fn(std::string_view name) {
  if (name.empty()) {
    return;
  }
  {
    code::Overlay& table = code::overlay();
    std::lock_guard<std::mutex> lock(table.mu);
    if (code::DynVerb* existing = code::find_dynamic_locked(&table, name)) {
      existing->eval = nullptr;
      existing->hidden = true;
      return;
    }
  }
  // find_op may run build_registry → install_script_verbs → register_fn; must
  // not hold overlay.mu across that path (non-recursive mutex).
  if (!code::find_op(name)) {
    return;
  }
  code::Overlay& table = code::overlay();
  std::lock_guard<std::mutex> lock(table.mu);
  if (code::DynVerb* existing = code::find_dynamic_locked(&table, name)) {
    existing->eval = nullptr;
    existing->hidden = true;
    return;
  }
  code::DynVerb hidden;
  hidden.name = std::string(name);
  hidden.hidden = true;
  table.dynamic.push_back(std::move(hidden));
}

std::optional<RegisteredFn> find_fn(std::string_view name) {
  if (name.empty()) {
    return std::nullopt;
  }
  {
    code::Overlay& table = code::overlay();
    std::lock_guard<std::mutex> lock(table.mu);
    if (const code::DynVerb* existing = code::find_dynamic_locked(&table, name)) {
      if (existing->hidden || !existing->eval) {
        return std::nullopt;
      }
      RegisteredFn fn;
      fn.name = existing->name;
      fn.params = existing->params;
      fn.param_count = existing->param_count;
      fn.eval = existing->eval;
      return fn;
    }
  }
  // Outside overlay.mu: find_op → registry() may install_host_verbs →
  // tile::install_script_verbs → register_fn (same mutex). Holding the lock
  // here throws resource_deadlock_would_occur (exit 0xC0000409 under harness).
  const code::OpDesc* desc = code::find_op(name);
  if (!desc) {
    return std::nullopt;
  }
  code::EvalFn eval = desc->eval;
  if (!eval) {
    code::Overlay& table = code::overlay();
    std::lock_guard<std::mutex> lock(table.mu);
    eval = code::find_exec_locked(table, desc->op);
  }
  if (!eval) {
    return std::nullopt;
  }
  RegisteredFn fn;
  fn.name = desc->name ? desc->name : std::string(name);
  fn.params = desc->params;
  fn.param_count = desc->param_count;
  fn.eval = eval;
  return fn;
}

}  // namespace py
}  // namespace content
