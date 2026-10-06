// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/plugin/product/report.h"

#include <windows.h>

#include <cstdio>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/pump/pump.h"
#include "app/views/harness/showcase/plugin/common/plugin_io.h"
#include "content/public/plugin_host.h"
#include "tool/command/command.h"
#include "plugin/runtime/host/capability/capability.h"
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
  plugin::ReportBridge* report = plugin::report_bridge(host);
  if (!report) {
    plugin_showcase_mark("report-open-fail");
    detach_maps(browser);
    return 1;
  }
  auto try_open = [&]() {
    const std::string open_json =
        std::string("{\"path\":\"") + json_escape_path(rep_path) + "\"}";
    tool::CommandArgs open_cmd;
    open_cmd.payload = open_json;
    return browser.plugins()->execute("report.open", open_cmd);
  };
  if (!try_open()) {
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
    if (!try_open()) {
      plugin_showcase_mark("report-open-fail");
      detach_maps(browser);
      return 1;
    }
    plugin_showcase_mark("report-fake-bridge");
  }
  plugin_showcase_mark("report-ok");
  pump_messages(400);

  const std::string post_json =
      "{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},"
      "{\"label\":\"Z\",\"value\":15}]}";
  tool::CommandArgs post_cmd;
  post_cmd.payload = post_json;
  if (!browser.plugins()->execute("report.post", post_cmd)) {
    (void)report->post(post_json);
  }
  pump_messages(400);

  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=report\n");
  return 0;
}

}  // namespace detail
}  // namespace app
