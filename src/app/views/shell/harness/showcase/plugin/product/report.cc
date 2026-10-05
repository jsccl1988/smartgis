// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/report.h"

#include <windows.h>

#include <cstdio>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/ui_delegate.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/common/plugin_io.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/browser/fake_report_browser.h"

namespace app {
namespace detail {
namespace {

plugin::FakeReportBrowser& report_showcase_fake() {
  static plugin::FakeReportBrowser fake;
  return fake;
}

}  // namespace

int run_report(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: report dock path\n");
  write_mark(kPluginShowcaseMarkLeaf, "report", /*truncate=*/true);

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  // FeatureInfo=0 … Report is typically inspector index 9 (layout composer).
  if (browser.ui()) {
    browser.ui()->activate_inspector_tab(9);
  }
  pump_messages(200);

  char rep_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {
      L"..\\data\\plugin\\report\\sample",
      L"data\\plugin\\report\\sample",
      L"..\\..\\testing\\data\\plugin\\report\\sample",
      L"..\\testing\\data\\plugin\\report\\sample"};
  if (!resolve_rel_under_exe(rels, 4, rep_path, sizeof(rep_path))) {
    plugin_showcase_mark("report-sample-missing");
    detach_maps(browser);
    return 1;
  }

  content::PluginHost* host =
      browser.plugins() ? browser.plugins()->host() : nullptr;
  if (!host) {
    plugin_showcase_mark("report-open-fail");
    detach_maps(browser);
    return 1;
  }
  if (!host->open_report(rep_path)) {
    plugin::FakeReportBrowser& fake = report_showcase_fake();
    fake.add_allowed_root(rep_path);
    host->set_report_bridge(
        [](std::string_view dir) {
          return report_showcase_fake().navigate(dir);
        },
        [](std::string_view json) {
          return report_showcase_fake().post_json(json);
        },
        []() { report_showcase_fake().close(); });
    if (!host->open_report(rep_path)) {
      plugin_showcase_mark("report-open-fail");
      detach_maps(browser);
      return 1;
    }
    plugin_showcase_mark("report-fake-bridge");
  }
  plugin_showcase_mark("report-ok");
  pump_messages(400);

  (void)host->post_to_report(
      "{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},"
      "{\"label\":\"Z\",\"value\":15}]}");
  pump_messages(400);

  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=report\n");
  return 0;
}

}  // namespace detail
}  // namespace app
