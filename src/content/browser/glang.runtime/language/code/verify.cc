// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/language/code/verify.h"

#include "base/core/log.h"

namespace content {
namespace code {
namespace {

void append_error(std::string* error, const char* message) {
  if (!error || !message) {
    return;
  }
  if (!error->empty()) {
    error->push_back('\n');
  }
  error->append(message);
}

}  // namespace

bool verify_module(const Module& module, std::string* error) {
  bool ok = true;
  for (const Inst& inst : module.insts) {
    if (inst.op != Op::kBuffer || inst.operand_count < 2) {
      continue;
    }
    const std::size_t begin = inst.operand_begin;
    if (begin + 1 >= module.operands.size()) {
      append_error(error, "buffer: missing geometry");
      LOGGING(LOG_ERROR, "code: buffer missing geometry line %u", inst.loc.line);
      ok = false;
      continue;
    }
    const int32_t left = module.operands[begin].crs_epsg;
    const int32_t right = module.operands[begin + 1].crs_epsg;
    if (left == 0 || right == 0) {
      append_error(error, "buffer: unknown crs");
      LOGGING(LOG_ERROR, "code: buffer unknown crs %d %d line %u", left, right,
              inst.loc.line);
      ok = false;
      continue;
    }
    if (left != right) {
      append_error(error, "buffer: crs mismatch");
      LOGGING(LOG_ERROR, "code: buffer crs mismatch %d %d line %u", left, right,
              inst.loc.line);
      ok = false;
    }
  }
  return ok;
}

}  // namespace code
}  // namespace content
