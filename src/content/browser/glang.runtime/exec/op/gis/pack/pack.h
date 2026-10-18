// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Slot write-back and EPSG reads shared by GIS opcode eval.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_PACK_PACK_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_PACK_PACK_H_

#include "content/browser/glang.runtime/language/code/op.h"

#include <string>

namespace content {
namespace code {
namespace detail {

bool put_result(const Inst& inst, SlotTable& slots, std::string value, std::string* error);

int32_t operand_epsg(const Module& module, const Inst& inst, int index);

bool store_ir(const Inst& inst,
              SlotTable& slots,
              bool ok,
              std::string value,
              const std::string& reason,
              std::string* error);

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_OP_GIS_PACK_PACK_H_
