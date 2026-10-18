// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_GATE_H_
#define PLUGIN_PRODUCT_WORLD3D_GATE_H_

namespace plugin {

// Atmosphere mode names the showcase gate checks before a suite script runs.
bool world3d_gate_contract(void* ctx);
bool world3d_gate_compute(void* ctx);

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_GATE_H_
