// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/scenario/run.h"

#include <windows.h>

#include <string>

#include "plugin/runtime/host/capability/shell.h"
#include "app/views/browser/ui_delegate.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/web/fake_report_browser.h"

namespace plugin {
namespace detail {
namespace {

plugin::FakeReportBrowser& report_showcase_fake() {
  static plugin::FakeReportBrowser fake;
  return fake;
}

}  // namespace

int run_report(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: report dock path\n");
  browser.mark_named(plugin::kMarkPlugin, "report", /*truncate=*/true);

  if (!browser.plugin_host()) {
    plugin_mark("plugins-fail");
    browser.detach_maps();
    return 1;
  }

  // FeatureInfo=0 … Report is typically inspector index 9 (layout composer).
  if (browser.ui()) {
    browser.ui()->activate_inspector_tab(9);
  }
  browser.pump(200);

  char rep_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {
      L"..\\data\\plugin\\report\\sample",
      L"data\\plugin\\report\\sample",
      L"..\\..\\testing\\data\\plugin\\report\\sample",
      L"..\\testing\\data\\plugin\\report\\sample"};
  if (!resolve_rel_under_exe(rels, 4, rep_path, sizeof(rep_path))) {
    plugin_mark("report-sample-missing");
    browser.detach_maps();
    return 1;
  }

  const std::string payload =
      std::string("{\"path\":\"") + json_escape_path(rep_path) + "\"}";
  if (dispatch_plugin_command(browser, "report.scenario.showcase", payload) !=
      0) {
    content::PluginHost* host =
        browser.plugin_host();
    plugin::ReportBridge* report = plugin::report_bridge(host);
    if (!host || !report) {
      plugin_mark("report-open-fail");
      browser.detach_maps();
      return 1;
    }
    plugin::FakeReportBrowser& fake = report_showcase_fake();
    fake.add_allowed_root(rep_path);
    report->set_bridges(
        [](std::string_view dir) {
          return report_showcase_fake().navigate(dir);
        },
        [](std::string_view json) {
          return report_showcase_fake().post_json(json);
        },
        []() { report_showcase_fake().close(); });
    if (dispatch_plugin_command(browser, "report.scenario.showcase", payload) !=
        0) {
      plugin_mark("report-open-fail");
      browser.detach_maps();
      return 1;
    }
    plugin_mark("report-fake-bridge");
  }
  plugin_mark("report-ok");
  browser.pump(400);

  plugin_mark("pass");
  browser.detach_maps();
  std::fprintf(stderr, "plugin-showcase: PASS mode=report\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin
