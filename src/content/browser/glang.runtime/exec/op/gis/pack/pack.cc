// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"

#include <utility>

namespace content {
namespace code {
namespace detail {

bool put_result(const Inst& inst, SlotTable& slots, std::string value, std::string* error) {
  if (inst.result_slot < 0) {
    return true;
  }
  return store_result(inst, slots, std::move(value), error);
}

int32_t operand_epsg(const Module& module, const Inst& inst, int index) {
  const Operand* operand = nth_operand(module, inst, index);
  return operand ? operand->crs_epsg : 0;
}

bool store_ir(const Inst& inst,
              SlotTable& slots,
              bool ok,
              std::string value,
              const std::string& reason,
              std::string* error) {
  if (!ok) {
    return fail_step(inst, reason.c_str(), error);
  }
  return put_result(inst, slots, std::move(value), error);
}

}  // namespace detail
}  // namespace code
}  // namespace content
