// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_REPORT_H_
#define APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_REPORT_H_

namespace app {

class Browser;

namespace detail {

// Inspector Report dock: open local HTML pack + post_json (plugin.report.il).
int run_report(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_PLUGIN_REPORT_H_
