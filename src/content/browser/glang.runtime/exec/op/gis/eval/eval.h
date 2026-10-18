// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GIS opcode eval functions. Registration stays in op/gis/dispatch.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_EVAL_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_EVAL_H_

#include "content/browser/glang.runtime/language/code/op.h"

#include <string>

namespace content {
struct CapabilityHost;
}

namespace content {
namespace code {
namespace detail {

bool eval_buffer(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error);

bool eval_simplify(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error);

bool eval_transform(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error);

bool eval_scan(content::CapabilityHost& host,
               const Module& module,
               const Inst& inst,
               SlotTable& slots,
               std::string* error);

bool eval_filter(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error);

bool eval_read_field(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error);

bool eval_project(content::CapabilityHost& host,
                  const Module& module,
                  const Inst& inst,
                  SlotTable& slots,
                  std::string* error);

bool eval_list_layers(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error);

bool eval_feature_count(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error);

bool eval_map_extent(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error);

bool eval_present_mode(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error);

bool eval_set_present_mode(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error);

bool eval_style_has(content::CapabilityHost& host,
                    const Module& module,
                    const Inst& inst,
                    SlotTable& slots,
                    std::string* error);

bool eval_style_clear(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error);

bool eval_style_summary(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error);

bool eval_list_analysis(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error);

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_EVAL_EVAL_H_
