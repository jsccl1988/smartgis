// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/self_test/self_test.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/pump/pump.h"
#include "app/views/runtime/capability/run_script.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/public/plugin_host.h"
#include "plugin/product/self_test/commands.h"
#include "plugin/product/self_test/shell.h"
#include "tool/command/command.h"

#include "base/trace/event/process_trace.h"

namespace app {
namespace {

class BrowserSelfTestShell final : public plugin::SelfTestShell {
 public:
  explicit BrowserSelfTestShell(Browser& browser) : browser_(browser) {}

  HWND hwnd() const override { return browser_.hwnd(); }
  ui::views::View* contents_view() const override {
    return browser_.contents_view();
  }
  ui::views::CatalogView* catalog_view() const override {
    return browser_.catalog_view();
  }
  ui::views::StatusBar* status_bar() const override {
    return browser_.status_bar();
  }
  ui::views::FeatureInfo* feature_info() const override {
    return browser_.feature_info();
  }
  ui::views::DrawHost* draw_host() const override {
    return browser_.draw_host();
  }
  ui::views::DrawHost* data_draw_host() const override {
    return browser_.data_draw_host();
  }
  ui::views::DrawHost* scene_draw_host() const override {
    return browser_.scene_draw_host();
  }
  content::ViewHost* edit_view_host() const override {
    return browser_.edit_view_host();
  }
  content::ViewHost* scene_host() override { return browser_.scene_host(); }
  content::MapScene* document() override { return browser_.document(); }
  content::Scene3dPresenter* scene3d() override { return browser_.scene3d(); }
  content::ViewFrame* view_frame() override { return browser_.view_frame(); }
  content::OrbitFrame* orbit_frame() override { return browser_.orbit_frame(); }
  content::Map2dPresenter* map2d() override { return browser_.map2d(); }

  bool run_tool_command(std::string_view command_id) override {
    return browser_.run_tool_command(command_id);
  }
  void refit_active_view() override { browser_.refit_active_view(); }
  void refresh_inspectors() override { browser_.refresh_inspectors(); }
  bool run_m2_self_test_hooks(std::string* err) override {
    return browser_.run_m2_self_test_hooks(err);
  }
  void select_map_tab(int index) override { browser_.select_map_tab(index); }
  void on_view_command(std::string_view command_id, int bookmark_index,
                       bool from_context, int view_x, int view_y) override {
    browser_.on_view_command(command_id, bookmark_index, from_context, view_x,
                             view_y);
  }
  void fit_map_extent() override { browser_.fit_map_extent(); }

  void pump(DWORD ms) override { detail::pump_messages(ms); }
  void mark(const char* step) override {
    detail::write_mark(detail::kSelfTestMarkLeaf, step, /*truncate=*/true);
  }
  void detach_maps() override { detail::detach_maps(browser_); }
  void stop_present_timers() override {
    detail::stop_map_present_timers(browser_);
  }
  bool capture_path(wchar_t* out, size_t cap, const wchar_t* leaf) override {
    return detail::exe_capture_path(out, cap, leaf);
  }
  bool capture_path_a(char* out, size_t cap, const char* leaf) override {
    return detail::exe_capture_path_a(out, cap, leaf);
  }
  bool exe_dir_slash(wchar_t* out, size_t cap) override {
    return detail::exe_dir_with_slash(out, cap);
  }

 private:
  Browser& browser_;
};

int dispatch_self_test_command(Browser& browser, const char* command_id) {
  PluginShell* plugins = browser.plugins();
  content::PluginHost* host = plugins ? plugins->host() : nullptr;
  if (!host || !command_id || !command_id[0]) {
    return 1;
  }
  BrowserSelfTestShell adapter(browser);
  (void)host->set_capability(plugin::kCapabilitySelfTest,
                             static_cast<void*>(&adapter));
  // Do not ensure_builtins(): that enables every product pack. Self-test only
  // needs this plugin's command table (same as the old in-app path).
  (void)plugin::register_self_test(host);
  (void)host->execute(command_id, tool::CommandArgs{});
  const int rc = plugin::self_test_last_exit_code();
  (void)host->set_capability(plugin::kCapabilitySelfTest, nullptr);
  return rc;
}

}  // namespace

int run_views_self_test(Browser& browser) {
  struct TraceDumpOnExit {
    ~TraceDumpOnExit() { base::trace::maybe_dump_tracing_to_env(); }
  } trace_dump_on_exit;
  (void)trace_dump_on_exit;
  return dispatch_self_test_command(browser, "self_test.run");
}

int run_views_console_self_test(Browser& browser) {
  if (try_run_suite_script(browser, "console", detail::kSelfTestMarkLeaf,
                           /*clear_marks=*/true)) {
    return 0;
  }
  return dispatch_self_test_command(browser, "self_test.console");
}

int console_self_test_body(Browser& browser) {
  return dispatch_self_test_command(browser, "self_test.console");
}

void pump_views_messages(DWORD ms) { detail::pump_messages(ms); }

}  // namespace app
