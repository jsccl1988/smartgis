// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_REPORT_GATE_H_
#define PLUGIN_PRODUCT_REPORT_GATE_H_

#include "plugin/runtime/host/capability/product_gate.h"

namespace plugin {

// Page stand-in: an HTML file is written only after the open-args contract
// passes. There is no HWND showcase for this package.
ProductGateResult run_report_product_gate();

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_REPORT_GATE_H_
