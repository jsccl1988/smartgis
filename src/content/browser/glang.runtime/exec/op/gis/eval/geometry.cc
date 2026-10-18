// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/eval/eval.h"

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"

#include <cstdint>
#include <string>
#include <utility>

namespace content {
namespace code {
namespace detail {

bool eval_buffer(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.buffer(arg_str_param(module, inst, slots, 0),
                                 operand_epsg(module, inst, 0),
                                 arg_str_param(module, inst, slots, 1),
                                 operand_epsg(module, inst, 1), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

bool eval_simplify(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.simplify(arg_str_param(module, inst, slots, 0),
                                   operand_epsg(module, inst, 0),
                                   arg_str_param(module, inst, slots, 1),
                                   operand_epsg(module, inst, 1), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

bool eval_transform(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
  const Operand* dst_op = nth_operand(module, inst, 1);
  int32_t dst = dst_op ? dst_op->crs_epsg : 0;
  if (dst == 0) {
    dst = arg_i32_param(module, inst, slots, 1);
  }
  std::string value;
  std::string reason;
  const bool ok = host.gis.transform(arg_str_param(module, inst, slots, 0),
                                    operand_epsg(module, inst, 0), dst, &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

}  // namespace detail
}  // namespace code
}  // namespace content
