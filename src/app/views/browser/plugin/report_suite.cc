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
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/paths.h"
#include "app/views/il.runtime/backend/mark.h"
#include "app/views/il.runtime/backend/dispatch.h"
#include "app/views/il.runtime/backend/pump.h"
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
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report",
                     /*truncate=*/true);

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    detail::write_mark(detail::kPluginShowcaseMarkLeaf, "plugins-fail", false);
    detail::detach_maps(browser);
    return 1;
  }

  if (browser.ui()) {
    browser.ui()->activate_inspector_tab(9);
  }
  detail::pump_messages(200);

  char rep_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {
      L"..\\data\\plugin\\report\\sample",
      L"data\\plugin\\report\\sample",
      L"..\\..\\testing\\data\\plugin\\report\\sample",
      L"..\\testing\\data\\plugin\\report\\sample"};
  if (!detail::resolve_first_existing_under_exe(rels, 4, rep_path,
                                                sizeof(rep_path))) {
    detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report-sample-missing",
                       false);
    detail::detach_maps(browser);
    return 1;
  }

  const std::string payload =
      std::string("{\"path\":\"") + json_escape_path(rep_path) + "\"}";
  if (detail::dispatch_plugin_command(browser, "report.scenario.showcase",
                                      payload) != 0) {
    content::PluginHost* host =
        browser.plugins() ? browser.plugins()->host() : nullptr;
    plugin::ReportBridge* report = plugin::report_bridge(host);
    if (!host || !report) {
      detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report-open-fail",
                         false);
      detail::detach_maps(browser);
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
    if (detail::dispatch_plugin_command(browser, "report.scenario.showcase",
                                        payload) != 0) {
      detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report-open-fail",
                         false);
      detail::detach_maps(browser);
      return 1;
    }
    detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report-fake-bridge",
                       false);
  }
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "report-ok", false);
  detail::pump_messages(400);
  detail::write_mark(detail::kPluginShowcaseMarkLeaf, "pass", false);
  detail::detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=report\n");
  return 0;
}

}  // namespace app
