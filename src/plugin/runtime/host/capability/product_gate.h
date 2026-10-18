// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_PRODUCT_GATE_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_PRODUCT_GATE_H_

#include <cstddef>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// One ordered acceptance step. `run` returns false to stop the chain.
// `name` is a stable literal (`contract`, `compute`, `command`, `visual`).
struct ProductGateStep {
  const char* name = nullptr;
  bool (*run)(void* ctx) = nullptr;
};

// `stopped_at` is nullptr when every step passed. Otherwise it points at the
// failing step's name literal (or `package` / `report` for runner failures).
struct ProductGateResult {
  bool passed = false;
  const char* stopped_at = nullptr;
};

// Runs `steps` in order and stops at the first false. Does not write a report.
PLUGIN_HOST_EXPORT ProductGateResult eval_product_gate(
    const char* package, const ProductGateStep* steps, size_t count, void* ctx);

// Writes `<exe>/captures/plugin/<package>/gate.json`. `baseline` is true only
// when `result.passed` is true. A BMP without that flag is not a baseline.
PLUGIN_HOST_EXPORT bool write_product_gate_report(const char* package,
                                                   const ProductGateResult& result);

inline ProductGateResult run_product_gate(const char* package,
                                           const ProductGateStep* steps,
                                           size_t count, void* ctx) {
  ProductGateResult result = eval_product_gate(package, steps, count, ctx);
  if (!write_product_gate_report(package, result)) {
    result.passed = false;
    if (result.stopped_at == nullptr) {
      result.stopped_at = "report";
    }
  }
  return result;
}

// Runs `head` (contract / compute / command). Calls `visual` only when every
// head step passed. Visual nonzero stops at `visual` and does not count as a
// baseline.
template <typename Visual>
int run_gated_visual(const char* package, const ProductGateStep* head,
                     size_t head_count, void* ctx, Visual&& visual) {
  ProductGateResult result = eval_product_gate(package, head, head_count, ctx);
  int rc = 1;
  if (result.passed) {
    rc = static_cast<int>(visual());
    if (rc != 0) {
      result.passed = false;
      result.stopped_at = "visual";
    }
  }
  if (!write_product_gate_report(package, result)) {
    result.passed = false;
    if (result.stopped_at == nullptr) {
      result.stopped_at = "report";
    }
    if (rc == 0) {
      rc = 1;
    }
  }
  return rc;
}

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_PRODUCT_GATE_H_
