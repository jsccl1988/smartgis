// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/plugin/dispatch.h"

#include <string>
#include <string_view>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/view/pixel/gate.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

namespace app {
namespace detail {
namespace {

class BrowserHarnessShell final : public plugin::HarnessShell {
 public:
  explicit BrowserHarnessShell(Browser& browser) : browser_(browser) {}

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
  content::ToolSession* edit_tool_session() const override {
    return browser_.edit_tool_session();
  }
  content::ToolSession* scene_tool_session() override { return browser_.scene_tool_session(); }
  content::GisScene* document() override { return browser_.document(); }
  content::Scene3dPresenter* scene3d() override { return browser_.scene3d(); }
  content::Scene3dStereoSession* scene3d_stereo() override {
    return browser_.scene3d_stereo();
  }
  content::ViewFrame* view_frame() override { return browser_.view_frame(); }
  content::OrbitFrame* orbit_frame() override { return browser_.orbit_frame(); }
  content::Map2dPresenter* map2d() override { return browser_.map2d(); }
  content::GisContents* gis_contents() override {
    return browser_.map_session();
  }
  content::PluginHost* plugin_host() override {
    PluginShell* plugins = browser_.plugins();
    return plugins ? plugins->host() : nullptr;
  }

  bool run_tool_command(std::string_view command_id) override {
    return browser_.run_tool_command(command_id);
  }
  void refit_active_view() override { browser_.refit_active_view(); }
  void refresh_inspectors() override { browser_.refresh_inspectors(); }
  bool run_m2_harness_hooks(std::string* err) override {
    return browser_.run_m2_harness_hooks(err);
  }
  void select_view_tab(int index) override { browser_.select_view_tab(index); }
  void on_view_command(std::string_view command_id, int bookmark_index,
                       bool from_context, int view_x, int view_y) override {
    browser_.on_view_command(command_id, bookmark_index, from_context, view_x,
                             view_y);
  }
  void fit_map_extent() override { browser_.fit_map_extent(); }

  void pump(DWORD ms) override { pump_messages(ms); }
  void mark(const char* step) override {
    // Append only. Horizon IL marks already truncated/seeded the leaf;
    // truncate=true here would wipe hwnd-ok / orbit-ok before gate score.
    write_mark(kHarnessMarkLeaf, step, /*truncate=*/false);
  }
  void mark_named(const wchar_t* leaf, const char* step,
                  bool truncate) override {
    write_mark(leaf, step, truncate);
  }
  void detach_views() override { ::app::detail::detach_views(browser_); }
  void finish_scene3d(bool borrowed_shell) override {
    ::app::detail::finish_scene3d(browser_, borrowed_shell);
  }
  void stop_present_timers() override {
    ::app::detail::stop_present_timers(browser_);
  }
  void resume_present_timers() override {
    ::app::detail::resume_present_timers(browser_);
  }
  void push_shared_extent() override { browser_.push_shared_extent(); }
  bool capture_path(wchar_t* out, size_t cap, const wchar_t* leaf) override {
    return exe_capture_path(out, cap, leaf);
  }
  bool capture_path_a(char* out, size_t cap, const char* leaf) override {
    return exe_capture_path_a(out, cap, leaf);
  }
  bool exe_dir_slash(wchar_t* out, size_t cap) override {
    return exe_dir_with_slash(out, cap);
  }
  bool bmp_has_visible_signal(const char* path_a, int* out_w,
                              int* out_h) override {
    BmpFileCheckOpts check;
    check.allow_32bpp = true;
    return bmp_file_has_visible_signal_a(path_a, out_w, out_h, check);
  }

  bool apply_plugin_frame(int index) override {
    return browser_.apply_plugin_frame(index);
  }
  bool preview_is_open() const override {
    return browser_.plugin_preview().is_open();
  }
  void preview_close() override { browser_.plugin_preview().close(); }
  bool preview_export_bmp(const char* path_utf8) override {
    if (!path_utf8 || !path_utf8[0]) {
      return false;
    }
    return browser_.plugin_preview().export_bmp(path_utf8);
  }

 private:
  Browser& browser_;
};

content::PluginHost* plugin_host(Browser& browser) {
  PluginShell* plugins = browser.plugins();
  return plugins ? plugins->host() : nullptr;
}

}  // namespace

int with_harness_shell(Browser& browser, int (*fn)(plugin::HarnessShell&)) {
  content::PluginHost* host = plugin_host(browser);
  if (!host || !fn) {
    return 1;
  }
  BrowserHarnessShell adapter(browser);
  (void)host->set_capability(plugin::kCapabilityHarness,
                             static_cast<void*>(&adapter));
  const int rc = fn(adapter);
  (void)host->set_capability(plugin::kCapabilityHarness, nullptr);
  return rc;
}

int dispatch_plugin_command(Browser& browser, const char* command_id,
                            std::string_view payload) {
  content::PluginHost* host = plugin_host(browser);
  if (!host || !command_id || !command_id[0]) {
    return 1;
  }
  PluginShell* shell = browser.plugins();
  BrowserHarnessShell adapter(browser);
  (void)host->set_capability(plugin::kCapabilityHarness,
                             static_cast<void*>(&adapter));
  // Dynamic: in-process pack self-reg + Registry/manifest LoadLibrary.
  // Do not ensure_builtins() (avoids enabling every appended builtin).
  if (shell) {
    (void)shell->ensure_command(command_id);
  }
  tool::CommandArgs args;
  args.payload = std::string(payload);
  const bool ok = host->execute(command_id, args);
  const std::string_view id(command_id);
  int rc = ok ? 0 : 1;
  if (id.find(".scenario") != std::string_view::npos) {
    // HarnessShell bodies publish via set_harness_scenario_exit. Prefer that
    // code even when execute() is false (e.g. prepare returned 50). Processing-
    // style commands (report) set exit 0 on success; default exit 1 must not
    // override a successful execute that forgot to clear it.
    const int harness_rc = plugin::harness_scenario_exit();
    if (ok) {
      if (harness_rc == 0 || harness_rc > 1) {
        rc = harness_rc;
      }
    } else if (harness_rc != 0) {
      rc = harness_rc;
    }
  }
  (void)host->set_capability(plugin::kCapabilityHarness, nullptr);
  return rc;
}

}  // namespace detail
}  // namespace app
