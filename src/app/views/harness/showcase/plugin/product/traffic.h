// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_TRAFFIC_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_TRAFFIC_H_

namespace app {

class Browser;

namespace detail {

// Map2d traffic.cost_path + framed BMP (peer plugin.traffic.il).
int run_traffic(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_TRAFFIC_H_
