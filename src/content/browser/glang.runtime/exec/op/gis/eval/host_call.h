// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Binds a GisCapability member to two WKT operands or one WKT operand.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_HOST_CALL_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_HOST_CALL_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"

#include <string>

namespace content {
namespace code {
namespace detail {

template <auto Method>
bool eval_host_binary(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  const std::string a = arg_str_param(module, inst, slots, 0);
  const std::string b = arg_str_param(module, inst, slots, 1);
  std::string value;
  std::string reason;
  const bool ok = (host.gis.*Method)(a, operand_epsg(module, inst, 0), b,
                                     operand_epsg(module, inst, 1), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

template <auto Method>
bool eval_host_unary(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = (host.gis.*Method)(arg_str_param(module, inst, slots, 0),
                                     operand_epsg(module, inst, 0), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_HOST_CALL_H_
