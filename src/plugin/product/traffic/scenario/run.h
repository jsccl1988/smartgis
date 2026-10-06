// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_TRAFFIC_SCENARIO_RUN_H_
#define PLUGIN_PRODUCT_TRAFFIC_SCENARIO_RUN_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Map2d traffic.cost_path + framed BMP (peer plugin.traffic.il).
int run_traffic(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_TRAFFIC_SCENARIO_RUN_H_
