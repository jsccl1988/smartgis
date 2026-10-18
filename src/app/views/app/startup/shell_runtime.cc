// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/shell_runtime.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/glang.runtime/host/view/host/capture_host.h"
#include "content/browser/glang.runtime/host/view/pixel/gate.h"
#include "content/browser/glang.runtime/exec/session/session.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/harness.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace {

class DrawHostPane final : public content::Pane {
 public:
  explicit DrawHostPane(ui::views::DrawHost* host = nullptr) : host_(host) {}

  void reset(ui::views::DrawHost* host) { host_ = host; }
  ui::views::DrawHost* raw() const { return host_; }

  Role role() const override {
    if (!host_) {
      return Role::kMapEdit;
    }
    return static_cast<Role>(host_->role());
  }
  AttachMode attach_mode() const override {
    if (!host_) {
      return AttachMode::kNone;
    }
    return static_cast<AttachMode>(host_->attach_mode());
  }
  bool is_visible() const override { return host_ && host_->is_visible(); }
  HWND native_view() const override {
    return host_ ? host_->native_view() : nullptr;
  }
  void sync_native_bounds() override {
    if (host_) {
      host_->sync_native_bounds();
    }
  }
  void invalidate_native() override {
    if (host_) {
      host_->invalidate_native();
    }
  }
  void set_gpu_present_visible(bool show) override {
    if (host_) {
      host_->set_gpu_present_visible(show);
    }
  }
  void resume_present_timer() override {
    if (host_) {
      host_->resume_present_timer();
    }
  }
  void pause_present() override {
    if (host_) {
      host_->pause_present();
    }
  }
  HWND present_hwnd() const override {
    return host_ ? host_->present_hwnd() : nullptr;
  }
  HWND input_hwnd() const override {
    return host_ ? host_->input_hwnd() : nullptr;
  }
  HWND identity_hud_hwnd() const override {
    return host_ ? host_->identity_hud_hwnd() : nullptr;
  }
  void request_frame() override {
    if (host_) {
      host_->request_frame();
    }
  }
  bool attach() override { return host_ && host_->attach(); }
  void detach() override {
    if (host_) {
      host_->detach();
    }
  }
  void realize_native() override {
    if (host_) {
      host_->realize_native();
    }
  }
  void sync_identity_frame() override {
    if (host_) {
      host_->sync_identity_frame();
    }
  }
  void* rhi_device() const override {
    return host_ ? host_->rhi_device() : nullptr;
  }
  bool wait_ready(std::uint32_t timeout_ms) override {
    return host_ && host_->wait_ready(timeout_ms);
  }
  bool last_gpu_present_ok() const override {
    return host_ && host_->last_gpu_present_ok();
  }
  std::uint32_t frame_presented() const override {
    return host_ ? host_->frame_presented() : 0;
  }
  std::uint32_t view_id() const override {
    return host_ ? host_->view_id() : 0;
  }
  content::GisContents* gis_contents() const override {
    return host_ ? host_->gis_contents() : nullptr;
  }
  float hud_fps() const override { return host_ ? host_->hud_fps() : 0.f; }
  ui::views::Rect bounds() const override {
    return host_ ? host_->bounds() : ui::views::Rect{};
  }
  ui::views::View* as_view() override { return host_; }

 private:
  ui::views::DrawHost* host_ = nullptr;
};

class BrowserShellBridge;

class BrowserHarnessShell final : public plugin::HarnessShell {
 public:
  BrowserHarnessShell(Browser& browser, BrowserShellBridge& bridge)
      : browser_(browser), bridge_(bridge) {}

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
  content::Pane* map_pane() override;
  content::Pane* data_pane() override;
  content::Pane* scene_pane() override;
  content::ToolSession* edit_tool_session() const override {
    return browser_.edit_tool_session();
  }
  content::ToolSession* scene_tool_session() override {
    return browser_.scene_tool_session();
  }
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
  void pump(DWORD ms) override { content::detail::pump_messages(ms); }
  void mark(const char* step) override {
    content::detail::write_mark(content::detail::kHarnessMarkLeaf, step, /*truncate=*/false);
  }
  void mark_named(const wchar_t* leaf, const char* step,
                  bool truncate) override {
    content::detail::write_mark(leaf, step, truncate);
  }
  void detach_views() override;
  void finish_scene3d(bool borrowed_shell) override;
  void stop_present_timers() override;
  void resume_present_timers() override;
  void push_shared_extent() override { browser_.push_shared_extent(); }
  bool capture_path(wchar_t* out, size_t cap, const wchar_t* leaf) override {
    return content::detail::exe_capture_path(out, cap, leaf);
  }
  bool capture_path_a(char* out, size_t cap, const char* leaf) override {
    return content::detail::exe_capture_path_a(out, cap, leaf);
  }
  bool exe_dir_slash(wchar_t* out, size_t cap) override {
    return content::detail::exe_dir_with_slash(out, cap);
  }
  bool bmp_has_visible_signal(const char* path_a, int* out_w,
                              int* out_h) override {
    content::detail::BmpFileCheckOpts check;
    check.allow_32bpp = true;
    return content::detail::bmp_file_has_visible_signal_a(path_a, out_w, out_h, check);
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
  BrowserShellBridge& bridge_;
};

class BrowserShellBridge final : public content::ShellBridge {
 public:
  explicit BrowserShellBridge(Browser& browser) : browser_(browser) {}

  content::GisScene* document() override { return browser_.document(); }
  content::Map2dPresenter* map2d() override { return browser_.map2d(); }
  content::Scene3dPresenter* scene3d() override { return browser_.scene3d(); }
  content::ViewFrame* view_frame() override { return browser_.view_frame(); }
  content::OrbitFrame* orbit_frame() override { return browser_.orbit_frame(); }
  content::BrowserSession& session() override { return browser_.session(); }
  HWND hwnd() const override { return browser_.hwnd(); }
  content::Pane* draw_host() override { return bind(map_, browser_.draw_host()); }
  content::Pane* data_draw_host() override {
    return bind(data_, browser_.data_draw_host());
  }
  content::Pane* scene_draw_host() override {
    return bind(scene_, browser_.scene_draw_host());
  }
  content::ToolSession* edit_tool_session() override {
    return browser_.edit_tool_session();
  }
  content::ToolSession* data_tool_session() override {
    return browser_.data_tool_session();
  }
  content::ToolSession* scene_tool_session() override {
    return browser_.scene_tool_session();
  }
  bool run_tool_command(std::string_view command_id) override {
    return browser_.run_tool_command(command_id);
  }
  void select_view_tab(int index) override { browser_.select_view_tab(index); }
  void on_view_command(std::string_view command_id, int bookmark_index,
                       bool from_context, int view_x, int view_y) override {
    browser_.on_view_command(command_id, bookmark_index, from_context, view_x,
                             view_y);
  }
  void fit_map_extent() override { browser_.fit_map_extent(); }
  void pump(std::uint32_t ms) override { content::detail::pump_messages(ms); }
  ui::views::View* contents_view() const override {
    return browser_.contents_view();
  }
  ui::views::CatalogView* catalog_view() override {
    return browser_.catalog_view();
  }
  ui::views::StatusBar* status_bar() override { return browser_.status_bar(); }
  content::GisContents* gis_contents() override {
    return browser_.map_session();
  }
  content::BlitFrameCache* blit() override { return browser_.blit(); }
  bool has_ui() const override { return browser_.ui() != nullptr; }
  void invalidate_native_map() override {
    if (browser_.ui()) {
      browser_.ui()->invalidate_native_map();
    }
  }
  void invalidate_map_overlays() override {
    if (browser_.ui()) {
      browser_.ui()->invalidate_map_overlays();
    }
  }
  void schedule_overlay_full_redraw() override {
    if (browser_.ui()) {
      browser_.ui()->schedule_overlay_full_redraw();
    }
  }
  void for_each_draw_host(const std::function<void(content::Pane*)>& fn) override {
    if (!browser_.ui() || !fn) {
      return;
    }
    browser_.ui()->for_each_draw_host([&](ui::views::DrawHost* raw) {
      DrawHostPane pane(raw);
      fn(raw ? &pane : nullptr);
    });
  }
  content::ToolSession* active_tool_session() override {
    return browser_.ui() ? browser_.ui()->active_tool_session() : nullptr;
  }
  content::Pane* active_map() override {
    return browser_.ui() ? bind(active_, browser_.ui()->active_map()) : nullptr;
  }
  void activate_inspector_tab(int index) override {
    if (browser_.ui()) {
      browser_.ui()->activate_inspector_tab(index);
    }
  }
  ui::views::TabStrip* inspector_tabs() override {
    return browser_.ui() ? browser_.ui()->inspector_tabs() : nullptr;
  }
  bool note_product_map2d_surface_after_show() override {
    return app::note_product_map2d_surface_after_show(browser_);
  }
  void ensure_china_maplibre_carto() override {
    app::ensure_china_maplibre_carto(browser_);
  }
  void frame_china_map2d(int view_w, int view_h) override {
    app::frame_china_map2d(browser_, view_w, view_h);
  }
  void apply_china_scene3d_product_defaults() override {
    app::apply_china_scene3d_product_defaults(browser_);
  }
  void apply_china_scene3d_orbit() override {
    app::apply_china_scene3d_orbit(browser_);
  }
  bool has_plugins() const override { return browser_.plugins() != nullptr; }
  void ensure_plugins() override {
    if (PluginShell* plugins = browser_.plugins()) {
      (void)plugins->ensure_discovered();
      (void)plugins->ensure_builtins();
    }
  }
  std::string eval_python(const std::string& code) override {
    PluginShell* plugins = browser_.plugins();
    return plugins ? plugins->eval_python(code) : "error: no plugin shell";
  }
  content::PluginHost* plugin_host() override {
    PluginShell* plugins = browser_.plugins();
    return plugins ? plugins->host() : nullptr;
  }
  bool ensure_command(const char* command_id) override {
    PluginShell* plugins = browser_.plugins();
    return plugins && command_id && plugins->ensure_command(command_id);
  }
  bool run_processing(const std::string& id, const std::string& args) override {
    PluginShell* plugins = browser_.plugins();
    return plugins && plugins->run_processing(id, args);
  }
  bool apply_plugin_frame(int index) override {
    return browser_.apply_plugin_frame(index);
  }
  int export_plugin_frames(const std::string& dir_leaf) override {
    return browser_.export_plugin_frames(dir_leaf);
  }
  int with_harness_shell(int (*fn)(plugin::HarnessShell&)) override {
    content::PluginHost* host = plugin_host();
    if (!host || !fn) {
      return 1;
    }
    BrowserHarnessShell adapter(browser_, *this);
    (void)host->set_capability(plugin::kCapabilityHarness,
                               static_cast<void*>(&adapter));
    const int rc = fn(adapter);
    (void)host->set_capability(plugin::kCapabilityHarness, nullptr);
    return rc;
  }
  int dispatch_plugin_command(const char* command_id,
                              std::string_view payload) override {
    content::PluginHost* host = plugin_host();
    if (!host || !command_id || !command_id[0]) {
      return 1;
    }
    BrowserHarnessShell adapter(browser_, *this);
    (void)host->set_capability(plugin::kCapabilityHarness,
                               static_cast<void*>(&adapter));
    if (PluginShell* plugins = browser_.plugins()) {
      (void)plugins->ensure_command(command_id);
    }
    tool::CommandArgs args;
    args.payload = std::string(payload);
    const bool ok = host->execute(command_id, args);
    const std::string_view id(command_id);
    int rc = ok ? 0 : 1;
    if (id.find(".scenario") != std::string_view::npos) {
      const int harness_rc = plugin::harness_exit();
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

 private:
  static content::Pane* bind(DrawHostPane& slot, ui::views::DrawHost* raw) {
    slot.reset(raw);
    return raw ? &slot : nullptr;
  }

  Browser& browser_;
  DrawHostPane map_;
  DrawHostPane data_;
  DrawHostPane scene_;
  DrawHostPane active_;
};

content::Pane* BrowserHarnessShell::map_pane() {
  return bridge_.draw_host();
}
content::Pane* BrowserHarnessShell::data_pane() {
  return bridge_.data_draw_host();
}
content::Pane* BrowserHarnessShell::scene_pane() {
  return bridge_.scene_draw_host();
}
void BrowserHarnessShell::detach_views() {
  content::detail::detach_views(static_cast<content::ShellBridge&>(bridge_));
}
void BrowserHarnessShell::finish_scene3d(bool borrowed_shell) {
  content::detail::finish_scene3d(static_cast<content::ShellBridge&>(bridge_),
                         borrowed_shell);
}
void BrowserHarnessShell::stop_present_timers() {
  content::detail::stop_present_timers(static_cast<content::ShellBridge&>(bridge_));
}
void BrowserHarnessShell::resume_present_timers() {
  content::detail::resume_present_timers(static_cast<content::ShellBridge&>(bridge_));
}

template <typename Fn, typename... Args>
auto call_bridge(Browser& browser, Fn fn, Args&&... args) {
  BrowserShellBridge bridge(browser);
  return fn(static_cast<content::ShellBridge&>(bridge),
            std::forward<Args>(args)...);
}

}  // namespace

content::ScriptStatus run_script(Browser& browser, const content::ScriptCall& call) {
  // smartgis is an inittab module. Py_Initialize must see that hook, so the
  // plugin runtime owns the first interpreter start.
  if (PluginShell* plugins = browser.plugins()) {
    plugins->ensure_python();
  }
  return call_bridge(
      browser,
      static_cast<content::ScriptStatus (*)(content::ShellBridge&,
                                            const content::ScriptCall&)>(
          &content::run_script),
      call);
}

int dispatch_plugin_command(Browser& browser,
                            const char* command_id,
                            std::string_view payload) {
  BrowserShellBridge bridge(browser);
  return bridge.dispatch_plugin_command(command_id, payload);
}

namespace detail {

void detach_views(Browser& browser) {
  BrowserShellBridge bridge(browser);
  content::detail::detach_views(static_cast<content::ShellBridge&>(bridge));
}

bool present_shell_scene3d_frame(ui::views::DrawHost* scene, DWORD wait_ms) {
  DrawHostPane pane(scene);
  return content::detail::present_shell_scene3d_frame(scene ? &pane : nullptr,
                                                      wait_ms);
}

HWND shell_scene3d_capture_hwnd(ui::views::DrawHost* scene) {
  DrawHostPane pane(scene);
  return content::detail::shell_scene3d_capture_hwnd(scene ? &pane : nullptr);
}

}  // namespace detail
}  // namespace app
