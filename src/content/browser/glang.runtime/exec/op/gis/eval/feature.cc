// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/eval/eval.h"

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"

#include <string>
#include <utility>

namespace content {
namespace code {
namespace detail {

bool eval_scan(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.scan_features(arg_str_param(module, inst, slots, 0), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

bool eval_filter(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.filter_features(arg_str_param(module, inst, slots, 0),
                                          arg_str_param(module, inst, slots, 1),
                                          arg_str_param(module, inst, slots, 2), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

bool eval_read_field(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.read_feature_field(arg_str_param(module, inst, slots, 0),
                                             arg_i32_param(module, inst, slots, 1),
                                             arg_str_param(module, inst, slots, 2), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

bool eval_project(content::CapabilityHost& host,
                  const Module& module,
                  const Inst& inst,
                  SlotTable& slots,
                  std::string* error) {
  std::string value;
  std::string reason;
  const bool ok = host.gis.project_features(arg_str_param(module, inst, slots, 0),
                                           arg_str_param(module, inst, slots, 1), &value, &reason);
  return store_ir(inst, slots, ok, std::move(value), reason, error);
}

}  // namespace detail
}  // namespace code
}  // namespace content
