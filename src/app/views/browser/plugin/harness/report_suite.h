// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_PLUGIN_HARNESS_REPORT_SUITE_H_
#define APP_VIEWS_BROWSER_PLUGIN_HARNESS_REPORT_SUITE_H_

namespace app {

class Browser;

// Chrome inspector tab + FakeReportBrowser fallback. Product open/post is
// report.open / report.post from the startup script.
int run_report_suite(Browser& browser);

}  // namespace app

#endif  // APP_VIEWS_BROWSER_PLUGIN_HARNESS_REPORT_SUITE_H_
