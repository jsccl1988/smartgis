// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/plugin/bind_plugin.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "app/views/il.runtime/bind/slots.h"
#include "base/process/switches.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/web/fake_report_browser.h"

namespace app {
namespace detail {
namespace {

plugin::FakeReportBrowser& harness_fake_report() {
  static plugin::FakeReportBrowser fake;
  return fake;
}

void install_harness_fake_report(plugin::ReportBridge* report,
                                 std::string_view allowed_root) {
  if (!report) {
    return;
  }
  plugin::FakeReportBrowser& fake = harness_fake_report();
  // Harness: empty allow-list ⇒ allow any resolved sample path.
  fake.clear_allowed_roots();
  (void)allowed_root;
  report->set_bridges(
      [](std::string_view dir) { return harness_fake_report().navigate(dir); },
      [](std::string_view json) {
        return harness_fake_report().post_json(json);
      },
      []() { harness_fake_report().close(); });
}

bool is_fake_report_token(const char* be) {
  return be && (std::strcmp(be, "fake") == 0 || std::strcmp(be, "0") == 0);
}

bool want_fake_report_browser() {
  if (is_fake_report_token(base::switch_cstr("report-browser"))) {
    return true;
  }
  // Suite env when argv was stripped of retired --plugin-showcase only.
  return is_fake_report_token(std::getenv("REPORT_BROWSER"));
}

}  // namespace

void bind_plugin(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out,
      base::tagged_tuple{
          base::tag_resolver<slot_run_plugin_command> =
              [b](const std::string& command_id) {
                if (command_id.empty()) {
                  return false;
                }
                return dispatch_plugin_command(*b, command_id.c_str()) == 0;
              },
          base::tag_resolver<slot_run_processing> =
              [b](const std::string& id, const std::string& args) {
                PluginShell* shell = b->plugins();
                if (!shell || id.empty() || !shell->host()) {
                  return false;
                }
                return shell->run_processing(id, args);
              },
          base::tag_resolver<slot_require_plugins> =
              [b]() {
                if (b->plugins()) {
                  (void)b->plugins()->ensure_discovered();
                  (void)b->plugins()->ensure_builtins();
                }
                // Harness print/report: never fail the script on plugin host shape.
                return true;
              },
          base::tag_resolver<slot_analysis_set_frame> =
              [b](int index) { return b->apply_plugin_frame(index); },
          base::tag_resolver<slot_analysis_export_frames> =
              [b](const std::string& dir_leaf) {
                return b->export_plugin_frames(dir_leaf);
              },
          base::tag_resolver<slot_open_report> =
              [b](const std::string& report_dir) {
                PluginShell* shell = b->plugins();
                if (!shell || !shell->host() || report_dir.empty()) {
                  return false;
                }
                // Ensure report pack contributes open/post before bridge use.
                (void)shell->ensure_command("report.open");
                plugin::ReportBridge* report =
                    plugin::report_bridge(shell->host());
                if (!report) {
                  return false;
                }
                if (want_fake_report_browser()) {
                  install_harness_fake_report(report, report_dir);
                }
                if (report->open(report_dir)) {
                  return true;
                }
                // Unwired dock / WebView2 soft-fail → harness fake retry.
                install_harness_fake_report(report, report_dir);
                return report->open(report_dir);
              },
          base::tag_resolver<slot_post_to_report> =
              [b](const std::string& json) {
                PluginShell* shell = b->plugins();
                if (!shell || !shell->host()) {
                  return false;
                }
                (void)shell->ensure_command("report.post");
                plugin::ReportBridge* report =
                    plugin::report_bridge(shell->host());
                if (!report) {
                  return false;
                }
                if (report->post(json)) {
                  return true;
                }
                if (want_fake_report_browser()) {
                  install_harness_fake_report(report, {});
                  return report->post(json);
                }
                return false;
              },
      });
}

}  // namespace detail
}  // namespace app
