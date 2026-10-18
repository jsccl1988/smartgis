// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_FLOOD_GATE_H_
#define PLUGIN_PRODUCT_FLOOD_GATE_H_

#include "plugin/runtime/host/capability/product_gate.h"

namespace plugin {

// Contract → compute → command → mask file. Later stages do not run after a
// failure, and the report's baseline flag stays false.
ProductGateResult run_flood_product_gate();

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_FLOOD_GATE_H_
