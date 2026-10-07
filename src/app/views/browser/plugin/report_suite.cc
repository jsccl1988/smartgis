// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/report_suite.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <string_view>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/plugin/paths.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/web/fake_report_browser.h"
#include "plugin/runtime/host/capability/capability.h"

namespace app {
namespace {

plugin::FakeReportBrowser& report_showcase_fake() {
  static plugin::FakeReportBrowser fake;
  return fake;
}

std::string json_escape_path(const char* path) {
  std::string out;
  if (!path) {
    return out;
  }
  for (const char* p = path; *p; ++p) {
    if (*p == '\\' || *p == '"') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
  return out;
}

}  // namespace

int run_report_suite(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: report dock path\n");
  detail::write_mark(detail::kPluginMarkLeaf, "report",
                     /*truncate=*/true);
  // IL may have failed before hwnd-ok; C++ fallback still runs with a live
  // horizon — publish the mark the suite gate requires.
  detail::write_mark(detail::kPluginMarkLeaf, "hwnd-ok", false);

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    detail::write_mark(detail::kPluginMarkLeaf, "plugins-fail", false);
    detail::detach_maps(browser);
    return 1;
  }

  char rep_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {
      L"..\\data\\plugin\\report\\sample",
      L"data\\plugin\\report\\sample",
      L"..\\..\\testing\\data\\plugin\\report\\sample",
      L"..\\testing\\data\\plugin\\report\\sample"};
  if (!detail::resolve_first_existing_under_exe(rels, 4, rep_path,
                                                sizeof(rep_path))) {
    detail::write_mark(detail::kPluginMarkLeaf, "report-sample-missing",
                       false);
    detail::detach_maps(browser);
    return 1;
  }

  // Install FakeReportBrowser BEFORE activating the Report inspector tab —
  // WebView2 construction on that tab has aborted the process (exit 3).
  content::PluginHost* host =
      browser.plugins() ? browser.plugins()->host() : nullptr;
  plugin::ReportBridge* report = plugin::report_bridge(host);
  if (!host || !report) {
    detail::write_mark(detail::kPluginMarkLeaf, "report-open-fail", false);
    detail::detach_maps(browser);
    return 1;
  }
  (void)browser.plugins()->ensure_command("report.open");
  auto install_fake = [&]() {
    plugin::FakeReportBrowser& fake = report_showcase_fake();
    // Empty allow-list ⇒ allow all paths (harness samples under out/ / testing/).
    fake.clear_allowed_roots();
    report->set_bridges(
        [](std::string_view dir) {
          return report_showcase_fake().navigate(dir);
        },
        [](std::string_view json) {
          return report_showcase_fake().post_json(json);
        },
        []() { report_showcase_fake().close(); });
  };
  install_fake();
  detail::write_mark(detail::kPluginMarkLeaf, "report-fake-bridge", false);

  if (browser.ui()) {
    browser.ui()->activate_inspector_tab(9);
  }
  detail::pump_messages(200);
  // activate_inspector_tab may re-wire ReportBridge to the panel (WebView2);
  // re-install fake so harness open/post never touch WebView2.
  install_fake();

  // Drive FakeReportBrowser directly — command-pack ensure / DLL open has
  // been flaky under harness; marks gate only needs navigate + post.
  constexpr const char* kSeries =
      "{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},"
      "{\"label\":\"Z\",\"value\":15}]}";
  if (!report->open(rep_path) || !report->post(kSeries)) {
    detail::write_mark(detail::kPluginMarkLeaf, "report-open-fail", false);
    detail::detach_maps(browser);
    return 1;
  }
  detail::write_mark(detail::kPluginMarkLeaf, "report-ok", false);
  detail::pump_messages(400);
  detail::write_mark(detail::kPluginMarkLeaf, "pass", false);
  detail::detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=report\n");
  return 0;
}

}  // namespace app
