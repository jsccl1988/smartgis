// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/gis/eval/eval.h"

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/language/ir/document.h"
#include "content/browser/glang.runtime/exec/op/gis/pack/pack.h"

#include <string>

namespace content {
namespace code {
namespace detail {

bool eval_list_layers(content::CapabilityHost& host,
                      const Module&,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  if (!host.document.layers_json) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, host.document.layers_json(), error);
}

bool eval_feature_count(content::CapabilityHost& host,
                        const Module&,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  const int count = ir::feature_count(host);
  if (count < 0) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, std::to_string(count), error);
}

bool eval_map_extent(content::CapabilityHost& host,
                     const Module&,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  if (!host.document.extent_json) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, host.document.extent_json(), error);
}

bool eval_present_mode(content::CapabilityHost& host,
                       const Module&,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  if (!host.view.present_mode) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, host.view.present_mode(), error);
}

bool eval_set_present_mode(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error) {
  if (!host.view.set_present_mode) {
    return fail_step(inst, "unbound", error);
  }
  const bool ok = host.view.set_present_mode(arg_str_param(module, inst, slots, 0));
  return put_result(inst, slots, ok ? "1" : "0", error);
}

bool eval_style_has(content::CapabilityHost& host,
                    const Module&,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error) {
  if (!host.document.has_style_document) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, host.document.has_style_document() ? "1" : "0", error);
}

bool eval_style_clear(content::CapabilityHost& host,
                      const Module&,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  if (!host.document.clear_style) {
    return fail_step(inst, "unbound", error);
  }
  host.document.clear_style();
  return put_result(inst, slots, "1", error);
}

bool eval_style_summary(content::CapabilityHost& host,
                        const Module&,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  if (!host.document.style_summary_json) {
    return fail_step(inst, "unbound", error);
  }
  return put_result(inst, slots, host.document.style_summary_json(), error);
}

}  // namespace detail
}  // namespace code
}  // namespace content
