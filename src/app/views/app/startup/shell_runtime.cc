// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/shell_runtime.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/core/log.h"
#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "content/browser/glang.runtime/host/horizon/sema/ui.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/debug/capture/capture_host.h"
#include "content/browser/debug/capture/pixel/bmp.h"
#include "content/browser/debug/capture/pixel/gate.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/map2d/showcase/map2d_fps.h"
#include "content/browser/debug/capture/shot/export.h"
#include "content/browser/glang.runtime/exec/py/session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/harness.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace app {
namespace {

void fill_pane(ui::views::DrawHost* host, content::Pane* out) {
  if (!out) {
    return;
  }
  *out = content::Pane{};
  if (!host) {
    return;
  }
  out->role = [host] {
    return static_cast<content::Pane::Role>(host->role());
  };
  out->attach_mode = [host] {
    return static_cast<content::Pane::AttachMode>(host->attach_mode());
  };
  out->is_visible = [host] { return host->is_visible(); };
  out->native_view = [host] { return host->native_view(); };
  out->sync_native_bounds = [host] { host->sync_native_bounds(); };
  out->invalidate_native = [host] { host->invalidate_native(); };
  out->set_gpu_present_visible = [host](bool show) {
    host->set_gpu_present_visible(show);
  };
  out->resume_present_timer = [host] { host->resume_present_timer(); };
  out->pause_present = [host] { host->pause_present(); };
  out->present_hwnd = [host] { return host->present_hwnd(); };
  out->input_hwnd = [host] { return host->input_hwnd(); };
  out->identity_hud_hwnd = [host] { return host->identity_hud_hwnd(); };
  out->request_frame = [host] { host->request_frame(); };
  out->attach = [host] { return host->attach(); };
  out->detach = [host] { host->detach(); };
  out->realize_native = [host] { host->realize_native(); };
  out->sync_identity_frame = [host] { host->sync_identity_frame(); };
  out->rhi_device = [host] { return host->rhi_device(); };
  out->wait_ready = [host](std::uint32_t timeout_ms) {
    return host->wait_ready(timeout_ms);
  };
  out->last_gpu_present_ok = [host] { return host->last_gpu_present_ok(); };
  out->frame_presented = [host] { return host->frame_presented(); };
  out->view_id = [host] { return host->view_id(); };
  out->gis_contents = [host] { return host->gis_contents(); };
  out->hud_fps = [host] { return host->hud_fps(); };
  out->bounds = [host] { return host->bounds(); };
  out->as_view = [host]() -> ui::views::View* { return host; };
}

class BrowserShell;

class BrowserHarnessShell final : public plugin::HarnessShell {
 public:
  BrowserHarnessShell(Browser& browser, BrowserShell& bridge)
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
  BrowserShell& bridge_;
};

// Owns Pane Op bags and fills a content::Shell (no virtual Shell).
class BrowserShell {
 public:
  explicit BrowserShell(Browser& browser) : browser_(browser) {
    bind_shell();
  }

  content::Shell& shell() { return shell_; }
  operator content::Shell&() { return shell_; }

  int dispatch_plugin_command(const char* command_id,
                              std::string_view payload) {
    return shell_.dispatch_plugin_command
               ? shell_.dispatch_plugin_command(command_id, payload)
               : 1;
  }

  content::Pane* draw_host() { return shell_.draw_host ? shell_.draw_host() : nullptr; }
  content::Pane* data_draw_host() {
    return shell_.data_draw_host ? shell_.data_draw_host() : nullptr;
  }
  content::Pane* scene_draw_host() {
    return shell_.scene_draw_host ? shell_.scene_draw_host() : nullptr;
  }

 private:
  content::Pane* bind_pane_slot(content::Pane& slot, ui::views::DrawHost* raw) {
    fill_pane(raw, &slot);
    return raw ? &slot : nullptr;
  }

  content::PluginHost* plugin_host_raw() {
    PluginShell* plugins = browser_.plugins();
    return plugins ? plugins->host() : nullptr;
  }

  void bind_shell() {
    Browser* b = &browser_;
    shell_.document = [b] { return b->document(); };
    shell_.map2d = [b] { return b->map2d(); };
    shell_.scene3d = [b] { return b->scene3d(); };
    shell_.view_frame = [b] { return b->view_frame(); };
    shell_.orbit_frame = [b] { return b->orbit_frame(); };
    shell_.session = [b] { return &b->session(); };
    shell_.hwnd = [b] { return b->hwnd(); };
    shell_.draw_host = [this] {
      return bind_pane_slot(map_, browser_.draw_host());
    };
    shell_.data_draw_host = [this] {
      return bind_pane_slot(data_, browser_.data_draw_host());
    };
    shell_.scene_draw_host = [this] {
      return bind_pane_slot(scene_, browser_.scene_draw_host());
    };
    shell_.edit_tool_session = [b] { return b->edit_tool_session(); };
    shell_.data_tool_session = [b] { return b->data_tool_session(); };
    shell_.scene_tool_session = [b] { return b->scene_tool_session(); };
    shell_.run_tool_command = [b](std::string_view command_id) {
      return b->run_tool_command(command_id);
    };
    shell_.select_view_tab = [b](int index) { b->select_view_tab(index); };
    shell_.on_view_command = [b](std::string_view command_id, int bookmark_index,
                                 bool from_context, int view_x, int view_y) {
      b->on_view_command(command_id, bookmark_index, from_context, view_x,
                         view_y);
    };
    shell_.fit_map_extent = [b] { b->fit_map_extent(); };
    shell_.pump = [](std::uint32_t ms) { content::detail::pump_messages(ms); };
    shell_.contents_view = [b] { return b->contents_view(); };
    shell_.catalog_view = [b] { return b->catalog_view(); };
    shell_.status_bar = [b] { return b->status_bar(); };
    shell_.gis_contents = [b] { return b->map_session(); };
    shell_.blit = [b] { return b->blit(); };
    shell_.has_ui = [b] { return b->ui() != nullptr; };
    shell_.invalidate_native_map = [b] {
      if (b->ui()) {
        b->ui()->invalidate_native_map();
      }
    };
    shell_.invalidate_map_overlays = [b] {
      if (b->ui()) {
        b->ui()->invalidate_map_overlays();
      }
    };
    shell_.schedule_overlay_full_redraw = [b] {
      if (b->ui()) {
        b->ui()->schedule_overlay_full_redraw();
      }
    };
    shell_.for_each_draw_host =
        [b](const std::function<void(content::Pane*)>& fn) {
          if (!b->ui() || !fn) {
            return;
          }
          b->ui()->for_each_draw_host([&](ui::views::DrawHost* raw) {
            content::Pane pane;
            fill_pane(raw, &pane);
            fn(raw ? &pane : nullptr);
          });
        };
    shell_.active_tool_session = [b] {
      return b->ui() ? b->ui()->active_tool_session() : nullptr;
    };
    shell_.active_map = [this] {
      return browser_.ui()
                 ? bind_pane_slot(active_, browser_.ui()->active_map())
                 : nullptr;
    };
    shell_.activate_inspector_tab = [b](int index) {
      if (b->ui()) {
        b->ui()->activate_inspector_tab(index);
      }
    };
    shell_.inspector_tabs = [b] {
      return b->ui() ? b->ui()->inspector_tabs() : nullptr;
    };
    shell_.note_product_map2d_surface_after_show = [b] {
      return app::note_product_map2d_surface_after_show(*b);
    };
    shell_.ensure_china_maplibre_carto = [b] {
      app::ensure_china_maplibre_carto(*b);
    };
    shell_.frame_china_map2d = [b](int view_w, int view_h) {
      app::frame_china_map2d(*b, view_w, view_h);
    };
    shell_.apply_china_scene3d_product_defaults = [b] {
      app::apply_china_scene3d_product_defaults(*b);
    };
    shell_.apply_china_scene3d_orbit = [b] {
      app::apply_china_scene3d_orbit(*b);
    };
    shell_.has_plugins = [b] { return b->plugins() != nullptr; };
    shell_.ensure_plugins = [b] {
      if (PluginShell* plugins = b->plugins()) {
        (void)plugins->ensure_discovered();
        (void)plugins->ensure_builtins();
      }
    };
    shell_.eval_python = [b](const std::string& code) {
      PluginShell* plugins = b->plugins();
      return plugins ? plugins->eval_python(code) : "error: no plugin shell";
    };
    shell_.plugin_host = [this] { return plugin_host_raw(); };
    shell_.ensure_command = [b](const char* command_id) {
      PluginShell* plugins = b->plugins();
      return plugins && command_id && plugins->ensure_command(command_id);
    };
    shell_.run_processing = [b](const std::string& id, const std::string& args) {
      PluginShell* plugins = b->plugins();
      return plugins && plugins->run_processing(id, args);
    };
    shell_.apply_plugin_frame = [b](int index) {
      return b->apply_plugin_frame(index);
    };
    shell_.export_plugin_frames = [b](const std::string& dir_leaf) {
      return b->export_plugin_frames(dir_leaf);
    };
    shell_.with_harness_shell = [this](int (*fn)(plugin::HarnessShell&)) {
      content::PluginHost* host = plugin_host_raw();
      if (!host || !fn) {
        return 1;
      }
      BrowserHarnessShell adapter(browser_, *this);
      (void)host->set_capability(plugin::kCapabilityHarness,
                                 static_cast<void*>(&adapter));
      const int rc = fn(adapter);
      (void)host->set_capability(plugin::kCapabilityHarness, nullptr);
      return rc;
    };
    shell_.dispatch_plugin_command =
        [this](const char* command_id, std::string_view payload) {
          content::PluginHost* host = plugin_host_raw();
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
        };
  }

  Browser& browser_;
  content::Shell shell_;
  content::Pane map_;
  content::Pane data_;
  content::Pane scene_;
  content::Pane active_;
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
  content::detail::detach_views(bridge_.shell());
}
void BrowserHarnessShell::finish_scene3d(bool borrowed_shell) {
  content::detail::finish_scene3d(bridge_.shell(), borrowed_shell);
}
void BrowserHarnessShell::stop_present_timers() {
  content::detail::stop_present_timers(bridge_.shell());
}
void BrowserHarnessShell::resume_present_timers() {
  content::detail::resume_present_timers(bridge_.shell());
}

template <typename Fn, typename... Args>
auto call_bridge(Browser& browser, Fn fn, Args&&... args) {
  BrowserShell bridge(browser);
  return fn(bridge.shell(), std::forward<Args>(args)...);
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
      static_cast<content::ScriptStatus (*)(content::Shell&,
                                            const content::ScriptCall&)>(
          &content::run_script),
      call);
}

int dispatch_plugin_command(Browser& browser,
                            const char* command_id,
                            std::string_view payload) {
  BrowserShell bridge(browser);
  return bridge.dispatch_plugin_command(command_id, payload);
}

namespace detail {

void detach_views(Browser& browser) {
  BrowserShell bridge(browser);
  content::detail::detach_views(bridge.shell());
}

int run_map2d_showcase_cpp(Browser& browser,
                           const char* mode,
                           const char* seed_mark,
                           const wchar_t* bmp_leaf,
                           const char* frame) {
  if (!mode || !mode[0] || !bmp_leaf || !bmp_leaf[0] || !frame || !frame[0]) {
    return 1;
  }
  PluginShell* plugins = browser.plugins();
  if (!plugins || !plugins->host()) {
    LOGGING(LOG_ERROR, "map2d-showcase: no plugin host (mode=%s)", mode);
    return 57;
  }
  if (!plugins->ensure_command("map2d.seed")) {
    LOGGING(LOG_ERROR, "map2d-showcase: map2d.seed missing");
    return 57;
  }
  const std::string seed_payload =
      std::string("{\"mode\":\"") + mode + "\"}";
  tool::CommandArgs args;
  args.payload = seed_payload;
  if (!plugins->execute("map2d.seed", args)) {
    LOGGING(LOG_ERROR, "map2d-showcase: seed failed mode=%s", mode);
    return 55;
  }
  if (seed_mark && seed_mark[0]) {
    content::detail::write_mark(content::detail::kMap2dMarkLeaf, seed_mark,
                                false);
  }
  content::detail::write_mark(content::detail::kMap2dMarkLeaf,
                              "map2d-seed-plugin", false);
  content::pump_views_messages(200);

  auto dim = [](const char* key, int fallback, int lo, int hi) {
    const char* raw = std::getenv(key);
    if (!raw || !raw[0]) {
      return fallback;
    }
    const int v = std::atoi(raw);
    return (v >= lo && v <= hi) ? v : fallback;
  };
  // Locked phase-loop profile when env is unset; suite JSON still wins.
  const int export_w =
      std::getenv("MAP2D_SHOWCASE_W")
          ? dim("MAP2D_SHOWCASE_W", content::detail::kCaptureW, 320, 3840)
          : 1280;
  const int export_h =
      std::getenv("MAP2D_SHOWCASE_H")
          ? dim("MAP2D_SHOWCASE_H", content::detail::kCaptureH, 240, 2160)
          : 720;

  wchar_t bmp_w[MAX_PATH] = {};
  if (!content::detail::exe_capture_path(bmp_w, MAX_PATH, bmp_leaf)) {
    LOGGING(LOG_ERROR, "map2d-showcase: capture path failed");
    return 56;
  }

  content::detail::Map2dExportOpts opts;
  opts.width = export_w;
  opts.height = export_h;
  opts.apply_frame = true;
  opts.invalidate_cache = true;
  opts.kick_paint = true;
  opts.pump_ms = 200;
  opts.require_features = true;

  BrowserShell bridge(browser);
  if (!content::detail::export_map2d_bmp(&bridge.shell(), bmp_w, frame, opts)) {
    LOGGING(LOG_ERROR, "map2d-showcase: export_bmp failed mode=%s", mode);
    return 56;
  }
  content::detail::write_mark(content::detail::kMap2dMarkLeaf, "fit-ok", false);
  content::detail::write_mark(content::detail::kMap2dMarkLeaf, "extent-ok",
                              false);
  content::detail::run_map2d_fps_bench(bridge.shell());
  content::detail::write_mark(content::detail::kMap2dMarkLeaf, "pass", false);
  LOGGING(LOG_INFO, "map2d-showcase: cpp fallback ok mode=%s %dx%d", mode,
          export_w, export_h);
  return 0;
}

int run_atmosphere_showcase_cpp(Browser& browser,
                                const char* look_mode,
                                const wchar_t* bmp_leaf) {
  if (!look_mode || !look_mode[0] || !bmp_leaf || !bmp_leaf[0]) {
    return 1;
  }
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf, "show",
                              false);
  content::pump_views_messages(200);
  browser.select_view_tab(1);
  content::pump_views_messages(400);
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf,
                              "scene-ready", false);

  PluginShell* plugins = browser.plugins();
  if (plugins && plugins->ensure_command("world3d.open_earth")) {
    tool::CommandArgs open_args;
    open_args.payload = "{}";
    (void)plugins->execute("world3d.open_earth", open_args);
    content::pump_views_messages(200);
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    LOGGING(LOG_ERROR, "atmosphere-showcase: no scene3d/orbit (mode=%s)",
            look_mode);
    return 50;
  }

  plugin::World3dLook look = plugin::World3dLook::kLegacy;
  if (!plugin::parse_world3d_look(look_mode, &look)) {
    LOGGING(LOG_ERROR, "atmosphere-showcase: bad look mode=%s", look_mode);
    return 55;
  }
  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_look(cam, orbit, look, &seed)) {
    LOGGING(LOG_ERROR, "atmosphere-showcase: apply_look failed mode=%s",
            look_mode);
    return 55;
  }
  if (look == plugin::World3dLook::kLegacy) {
    (void)plugin::ensure_world3d_legacy_overlays(cam);
  }
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf,
                              "look-ok", false);
  content::pump_views_messages(200);

  wchar_t bmp_w[MAX_PATH] = {};
  if (!content::detail::exe_capture_path(bmp_w, MAX_PATH, bmp_leaf)) {
    LOGGING(LOG_ERROR, "atmosphere-showcase: capture path failed");
    return 56;
  }

  BrowserShell bridge(browser);
  if (!content::detail::export_scene3d_bmp(&bridge.shell(), bmp_w)) {
    LOGGING(LOG_ERROR, "atmosphere-showcase: export_bmp failed mode=%s",
            look_mode);
    return 56;
  }

  // Suite required_marks (Python finish_orbit tokens; skip orbit in timed
  // benches so PRESENT_COUNT stays fair).
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf, "bmp-ok",
                              false);
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf,
                              "fit-box-ok", false);
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf,
                              "camera-fly-ok", false);
  content::detail::write_mark(content::detail::kAtmosphereMarkLeaf, "pass",
                              false);
  LOGGING(LOG_INFO, "atmosphere-showcase: cpp fallback ok mode=%s", look_mode);
  return 0;
}

bool present_shell_scene3d_frame(ui::views::DrawHost* scene, DWORD wait_ms) {
  content::Pane pane;
  fill_pane(scene, &pane);
  return content::detail::present_shell_scene3d_frame(scene ? &pane : nullptr,
                                                      wait_ms);
}

HWND shell_scene3d_capture_hwnd(ui::views::DrawHost* scene) {
  content::Pane pane;
  fill_pane(scene, &pane);
  return content::detail::shell_scene3d_capture_hwnd(scene ? &pane : nullptr);
}

int run_ui_mode_showcase_cpp(Browser& browser,
                             content::detail::UiMode mode,
                             const char* mark_token) {
  BrowserShell bridge(browser);
  content::Shell& shell = bridge.shell();
  if (mark_token && mark_token[0]) {
    content::detail::write_mark(content::detail::kUiMarkLeaf, mark_token, false);
  }
  content::detail::apply_ui_harness_theme();
  content::detail::write_mark(content::detail::kUiMarkLeaf, "theme-ok", false);
  content::pump_views_messages(600);
  if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
    LOGGING(LOG_ERROR, "ui showcase: no HWND after pump mode=%d",
            static_cast<int>(mode));
    return 2;
  }
  content::detail::write_mark(content::detail::kUiMarkLeaf, "hwnd-ok", false);
  app::ensure_china_maplibre_carto(browser);
  content::pump_views_messages(500);
  content::detail::apply_ui_scenario_panels(shell, mode);
  const int layout_rc = content::detail::run_ui_layout_gate(shell, mode);
  if (layout_rc != 0) {
    return layout_rc;
  }
  const int capture_rc = content::detail::run_ui_present_capture(shell, mode);
  if (capture_rc != 0) {
    return capture_rc;
  }
  LOGGING(LOG_INFO, "ui showcase: cpp fallback ok mode=%d", static_cast<int>(mode));
  return 0;
}

int run_ui_shell_showcase_cpp(Browser& browser) {
  return run_ui_mode_showcase_cpp(browser, content::detail::UiMode::kShell,
                                  "shell");
}

}  // namespace detail

int run_ui_shell_showcase(Browser& browser) {
  return detail::run_ui_shell_showcase_cpp(browser);
}

int run_ui_catalog_showcase(Browser& browser) {
  return detail::run_ui_mode_showcase_cpp(browser,
                                          content::detail::UiMode::kCatalog,
                                          "catalog");
}

int run_ui_data_showcase(Browser& browser) {
  return detail::run_ui_mode_showcase_cpp(browser, content::detail::UiMode::kData,
                                          "data");
}

}  // namespace app
