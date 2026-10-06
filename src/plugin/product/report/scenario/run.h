// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_REPORT_SCENARIO_RUN_H_
#define PLUGIN_PRODUCT_REPORT_SCENARIO_RUN_H_

namespace plugin {

class HarnessShell;

namespace detail {

// Inspector Report dock: open local HTML pack + post_json (plugin.report.il).
int run_report(HarnessShell& browser);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_REPORT_SCENARIO_RUN_H_
