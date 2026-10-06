// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_REPORT_SUITE_H_
#define APP_VIEWS_BROWSER_PLUGIN_REPORT_SUITE_H_

namespace app {

class Browser;

// Chrome inspector tab + FakeReportBrowser fallback. Product open/post is
// plugin command report.scenario.showcase.
int run_report_suite(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_REPORT_SUITE_H_
