// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/extend/exec.h"

#include "content/browser/glang.runtime/exec/op/register.h"

#include <charconv>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace content {
namespace code {
namespace {

struct ExtensionTable {
  std::vector<LanguageExtension> languages;
  std::vector<ExecExtension> execs;
};

ExtensionTable& extensions() {
  static ExtensionTable table;
  return table;
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

void register_language_extension(const LanguageExtension& ext) {
  if (!ext.name || !ext.name[0] || !ext.lower_call) {
    return;
  }
  ExtensionTable& table = extensions();
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
  ExtensionTable& table = extensions();
  for (ExecExtension& slot : table.execs) {
    if (slot.op == ext.op) {
      slot = ext;
      return;
    }
  }
  table.execs.push_back(ext);
}

EvalFn find_exec_extension(Op op) {
  const ExtensionTable& table = extensions();
  for (const ExecExtension& slot : table.execs) {
    if (slot.op == op) {
      return slot.eval;
    }
  }
  return nullptr;
}

bool lower_extended_call(const CallLowering& call,
                         Module* extra,
                         std::string* error) {
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
  const ExtensionTable& table = extensions();
  for (const LanguageExtension& slot : table.languages) {
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
  EvalFn eval = desc->eval ? desc->eval : find_exec_extension(desc->op);
  if (!eval) {
    if (error) {
      *error = "no eval";
    }
    LOGGING(LOG_ERROR, "gis call: no eval for %s", desc->name ? desc->name : "op");
    return false;
  }
  if (!eval(host, module, module.insts.front(), slots, error)) {
    return false;
  }
  if (result && !slots.values.empty()) {
    *result = slots.values.front();
  }
  return true;
}

}  // namespace code
}  // namespace content
