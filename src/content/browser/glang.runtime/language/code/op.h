// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Opcode descriptors. Eval is a function pointer; the interpreter does not
// switch on script names.

#ifndef IL_RUNTIME_CODEGEN_CODE_OP_H_
#define IL_RUNTIME_CODEGEN_CODE_OP_H_

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include "content/browser/glang.runtime/language/code/module.h"
#include "base/core/log.h"

namespace content {
struct CapabilityHost;
}

namespace content {
namespace code {

using EvalFn = bool (*)(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error);

// One verb. aliases is a nullptr-terminated list, or null when there are none.
// produces_slot is true only for resolve_data, capture_path, and sidecar_path.
struct OpDesc {
  Op op;
  const char* name;
  const char* const* aliases;
  const Param* params;
  int param_count;
  bool produces_slot;
  EvalFn eval;
};

// Canonical script name and aliases. Unknown names return null.
const OpDesc* find_op(std::string_view script_name);
const OpDesc* find_op(Op op);

// Builds the opcode table once. Later calls do not allocate.
void ensure_ops_registered();

inline const Operand* nth_operand(const Module& module,
                                  const Inst& inst,
                                  int index) {
  if (index < 0 || index >= static_cast<int>(inst.operand_count)) {
    return nullptr;
  }
  const std::size_t at = static_cast<std::size_t>(inst.operand_begin) +
                         static_cast<std::size_t>(index);
  if (at >= module.operands.size()) {
    return nullptr;
  }
  return &module.operands[at];
}

inline bool step_fail(const char* name, const char* why, std::string* error) {
  const char* reason = why && why[0] ? why : "failed";
  const char* verb = name && name[0] ? name : "op";
  if (error) {
    *error = std::string(verb) + ": " + reason;
  }
  LOGGING(LOG_ERROR, "code: %s: %s", verb, reason);
  return false;
}

inline const char* op_name(Op op) {
  const OpDesc* desc = find_op(op);
  if (desc && desc->name && desc->name[0]) {
    return desc->name;
  }
  return "op";
}

inline bool ok_step(const Inst& inst, bool ok, std::string* error) {
  if (ok) {
    return true;
  }
  return step_fail(op_name(inst.op), "failed", error);
}

inline bool fail_step(const Inst& inst, const char* why, std::string* error) {
  return step_fail(op_name(inst.op), why, error);
}

inline bool store_result(const Inst& inst,
                         SlotTable& slots,
                         std::string value,
                         std::string* error) {
  if (inst.result_slot < 0) {
    return fail_step(inst, "failed", error);
  }
  const std::size_t index = static_cast<std::size_t>(inst.result_slot);
  if (slots.values.size() <= index) {
    slots.values.resize(index + 1);
  }
  slots.values[index] = std::move(value);
  return true;
}

inline const Param* param_of(const Inst& inst, int index) {
  const OpDesc* desc = find_op(inst.op);
  if (!desc || !desc->params || index < 0 || index >= desc->param_count) {
    return nullptr;
  }
  return &desc->params[index];
}

inline int32_t arg_i32(const Module& module,
                       const Inst& inst,
                       const SlotTable& slots,
                       int index,
                       int32_t fallback) {
  const Operand* operand = nth_operand(module, inst, index);
  if (!operand) {
    return fallback;
  }
  if (operand->kind == ArgKind::kI32 || operand->kind == ArgKind::kBool) {
    return operand->i32;
  }
  const std::string text = expand_operand(module, *operand, slots);
  if (text.empty()) {
    return fallback;
  }
  return std::atoi(text.c_str());
}

inline int32_t arg_i32_param(const Module& module,
                             const Inst& inst,
                             const SlotTable& slots,
                             int index) {
  const Param* param = param_of(inst, index);
  const int32_t fallback = param ? param->default_i32 : 0;
  return arg_i32(module, inst, slots, index, fallback);
}

inline bool arg_bool_param(const Module& module,
                           const Inst& inst,
                           const SlotTable& slots,
                           int index) {
  return arg_i32_param(module, inst, slots, index) != 0;
}

inline std::string arg_str(const Module& module,
                           const Inst& inst,
                           const SlotTable& slots,
                           int index,
                           const char* fallback) {
  const Operand* operand = nth_operand(module, inst, index);
  if (!operand) {
    return fallback ? fallback : "";
  }
  if (operand->kind == ArgKind::kI32 || operand->kind == ArgKind::kBool) {
    return std::to_string(operand->i32);
  }
  return expand_operand(module, *operand, slots);
}

inline std::string arg_str_param(const Module& module,
                                 const Inst& inst,
                                 const SlotTable& slots,
                                 int index) {
  const Param* param = param_of(inst, index);
  const char* fallback = param && param->default_str ? param->default_str : "";
  return arg_str(module, inst, slots, index, fallback);
}

// Slot value for a var= operand: i32 index, path-slot sentinel, or slot name.
inline std::string slot_from_operand(const Module& module,
                                     const Operand& operand,
                                     const SlotTable& slots) {
  if (operand.kind == ArgKind::kI32 || operand.kind == ArgKind::kBool) {
    if (operand.i32 < 0) {
      return {};
    }
    const auto index = static_cast<std::size_t>(operand.i32);
    if (index < slots.values.size()) {
      return slots.values[index];
    }
    return {};
  }
  if (operand.aux == 0 && operand.i32 < 0 &&
      (operand.kind == ArgKind::kPath || operand.kind == ArgKind::kStr)) {
    if (operand.index < slots.values.size()) {
      return slots.values[operand.index];
    }
    return {};
  }
  const std::string key = expand_operand(module, operand, slots);
  if (key.empty()) {
    return {};
  }
  for (std::size_t i = 0; i < module.slot_names.size(); ++i) {
    if (module.slot_names[i] == key) {
      if (i < slots.values.size()) {
        return slots.values[i];
      }
      return {};
    }
  }
  return {};
}

// var= wins when that slot is non-empty; otherwise the path string.
inline std::string bound_path(const Module& module,
                              const Inst& inst,
                              const SlotTable& slots,
                              int path_index,
                              int var_index) {
  if (const Operand* var_op = nth_operand(module, inst, var_index)) {
    const std::string from_var = slot_from_operand(module, *var_op, slots);
    if (!from_var.empty()) {
      return from_var;
    }
  }
  return arg_str_param(module, inst, slots, path_index);
}

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_CODE_OP_H_
