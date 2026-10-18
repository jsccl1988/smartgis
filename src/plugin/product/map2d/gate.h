// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_MAP2D_GATE_H_
#define PLUGIN_PRODUCT_MAP2D_GATE_H_

namespace plugin {

// Seed-mode contract and name round-trip. The harness adds the command step
// and only then runs a showcase capture.
bool map2d_gate_contract(void* ctx);
bool map2d_gate_compute(void* ctx);

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_MAP2D_GATE_H_
