// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_FLOOD_SCENARIO_RUN_H_
#define PLUGIN_PRODUCT_FLOOD_SCENARIO_RUN_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Map2d flood.inundate + framed BMP (peer plugin.flood.il).
int run_flood(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_FLOOD_SCENARIO_RUN_H_
