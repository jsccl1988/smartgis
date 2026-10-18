// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/browser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_contents.h"
#include "content/public/plugin_host.h"
#include "content/public/tool_session.h"
#include "gis/style/document/style_document.h"
#include "vista/component/map/carto/resolve.h"
#include "vista/component/map/layout.h"

#if defined(_DEBUG)
#include <crtdbg.h>
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {

Browser::Browser() : session_(content::BrowserSession::create()) {}

Browser::~Browser() {
  prepare_close();
  session_->clear_gis_contents_observer();
  if (plugins_) {
    plugins_->shutdown();
    plugins_.reset();
  }
  ui_.reset();
}

void Browser::set_plugins_dir(std::string path) {
  plugins_dir_ = std::move(path);
}

void Browser::set_defer_china_seed(bool defer) {
  defer_china_seed_ = defer;
}

bool Browser::defer_china_seed() const {
  return defer_china_seed_;
}

void Browser::set_enable_oop_render(bool enable) {
  enable_oop_render_ = enable;
}

bool Browser::enable_oop_render() const {
  return enable_oop_render_;
}

content::Scene3dPresenter* Browser::scene3d() {
  return &session_->scene3d();
}

const content::Scene3dPresenter* Browser::scene3d() const {
  return &session_->scene3d();
}

content::GisScene* Browser::document() {
  return &session_->document();
}

const content::GisScene* Browser::document() const {
  return &session_->document();
}

content::ViewFrame* Browser::view_frame() {
  return &session_->view_frame();
}

const content::ViewFrame* Browser::view_frame() const {
  return &session_->view_frame();
}

content::OrbitFrame* Browser::orbit_frame() {
  return &session_->orbit_frame();
}

const content::OrbitFrame* Browser::orbit_frame() const {
  return &session_->orbit_frame();
}

content::Map2dPresenter* Browser::map2d() {
  return &session_->map2d();
}

const content::Map2dPresenter* Browser::map2d() const {
  return &session_->map2d();
}

content::Scene3dStereoSession* Browser::scene3d_stereo() {
  return &session_->scene3d_stereo();
}

content::BlitFrameCache* Browser::blit() {
  return &session_->blit();
}

content::ViewNavigation* Browser::navigation() {
  return &session_->navigation();
}

const content::ViewNavigation* Browser::navigation() const {
  return &session_->navigation();
}

content::GisContents* Browser::map_session() {
  return session_->gis_contents();
}

content::ToolSession* Browser::edit_tool_session() {
  return session_->edit_tool_session();
}

content::ToolSession* Browser::data_tool_session() {
  return session_->data_tool_session();
}

content::ToolSession* Browser::scene_tool_session() {
  return session_->scene_tool_session();
}

content::GisHwndGestures* Browser::edit_gestures() {
  return &session_->edit_gestures();
}

content::GisHwndGestures* Browser::data_gestures() {
  return &session_->data_gestures();
}

content::GisHwndGestures* Browser::scene_gestures() {
  return &session_->scene_gestures();
}

PluginShell* Browser::plugins() {
  return plugins_.get();
}

const PluginShell* Browser::plugins() const {
  return plugins_.get();
}

content::EventBus::Connection* Browser::selection_sub() {
  return &selection_sub_;
}

content::EventBus::Connection* Browser::edit_sub() {
  return &edit_sub_;
}

content::EventBus::Connection* Browser::extent_sub() {
  return &extent_sub_;
}

content::EventBus::Connection* Browser::layers_sub() {
  return &layers_sub_;
}

bool Browser::init() {
  BASE_TRACE_EVENT("Browser.init.body", "startup");
  {
    BASE_TRACE_EVENT("Session.init_tool_sessions", "startup");
    LOGGING(LOG_INFO, "startup: session.init_tool_sessions");
    session_->init_tool_sessions();
    if (enable_oop_render_) {
      if (!session_->ensure_oop_render_process()) {
        LOGGING(LOG_WARNING,
                "startup: enable_oop_render requested but StartRenderProcess "
                "failed continuing in-process");
      }
    }
  }

  {
    BASE_TRACE_EVENT("PluginShell.init", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell.init");
    plugins_ = std::make_unique<PluginShell>();
    plugins_->set_plugins_dir(plugins_dir_);
    if (!session_->edit_tool_session() ||
        !plugins_->init(session_->edit_tool_session()->events())) {
      LOGGING(LOG_ERROR, "startup: PluginShell.init failed");
      if (plugins_) {
        plugins_->shutdown();
        plugins_.reset();
      }
      // Showcase / self-test can still paint map2d without plugins; product
      // interactive shell keeps hard-fail by returning false below when UI
      // creation also requires plugins. Soft-continue so horizon can load.
      LOGGING(LOG_WARNING, "startup: continuing without PluginShell");
    }
  }

  if (plugins_) {
    // Build GisContents and PluginHost before the map display thread starts
    // inside create_browser_ui. Those member maps race the display thread's
    // first allocs and the next LogSink new reports 0xC0000374.
    const bool have_contents = session_->ensure_gis_contents();
    LOGGING(LOG_INFO, "startup: gis contents %s",
            have_contents ? "created" : "missing");
    if (!(have_contents &&
          plugins_->attach_gis_contents(session_->gis_contents()))) {
      LOGGING(LOG_WARNING,
              "startup: GisContents/PluginHost attach failed; no bridges");
    }
  }

  {
    BASE_TRACE_EVENT("CartoStyle", "startup");
    const gis::style::StyleDocument* carto = vista::default_carto_style();
    const bool carto_ok = carto && !carto->layers.empty();
    const bool print_ok = !vista::print_carto_style_json().empty();
    if (!carto_ok || !print_ok) {
      LOGGING(LOG_ERROR,
              "startup: carto style resource missing (ui/carto)");
    } else {
      LOGGING(LOG_INFO, "startup: carto style resource loaded (%u layers)",
              static_cast<unsigned>(carto->layers.size()));
    }
  }

  {
    BASE_TRACE_EVENT("CreateBrowserUi", "startup");
    LOGGING(LOG_INFO, "startup: create_browser_ui");
    ui_ = create_browser_ui(this);
    if (!ui_) {
      LOGGING(LOG_ERROR, "startup: create_browser_ui failed");
      return false;
    }
#if defined(_DEBUG)
    // NT heap damage during BrowserView member ctors is reported on the next
    // LogSink alloc (gis contents created). Pin which side owns it.
    if (!_CrtCheckMemory()) {
      LOGGING(LOG_ERROR, "startup: heap corrupt after browser ui");
    } else {
      LOGGING(LOG_INFO, "startup: heap ok after browser ui");
    }
#endif
  }

  if (plugins_ && plugins_->host()) {
    wire_plugin_present_dataset();
    install_plugin_host_bridges();
  }

  BASE_TRACE_EVENT("InitShell", "startup");
  LOGGING(LOG_INFO, "startup: init_shell");
  const bool ok = ui_->init_shell();
  if (!ok) {
    LOGGING(LOG_ERROR, "startup: init_shell failed");
  }
  return ok;
}

namespace {

bool seh_fit_map_extent(Browser* browser) {
  if (!browser) {
    return false;
  }
  __try {
    browser->fit_map_extent();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace

void Browser::show() {
  if (ui_) {
    // show_shell shows the shell and schedules the first map invalidate.
    // Full first-map present wait is opt-in (SYNC_FIRST_MAP_PRESENT=1).
    ui_->show_shell();
  }
  // Fit after horizon is visible (final client size). show_shell may already
  // have pumped a first carto layout (layout_build_count > 0) at a pre-reseed
  // size / FlyCube latch — do not skip fit on that count or DefaultChina keeps
  // the smeared HWND face. Harness (skip-ambox-catalog): demo-only seed AVs
  // inside fit_map_extent. Scene3D framing is applied later via product defaults.
  const bool skip_fit = []() {
    const char* skip = base::switch_cstr("skip-ambox-catalog");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  if (!skip_fit) {
    if (!seh_fit_map_extent(this)) {
      LOGGING(LOG_WARNING, "startup: Browser::show fit_map_extent SEH");
    }
  }
  // Sync China: always drop latch + invalidate so the post-fit present records
  // final-client carto (gate on layout_build_count contradicted this).
  if (note_product_map2d_surface_after_show(*this) && ui_) {
    ui_->invalidate_native_map();
  }
  // Opt-in dump after fit+reset (overwrites show_shell's early pre-fit dump).
  if (const char* dump = base::switch_cstr("map2d-dump-bmp")) {
    if (dump[0]) {
      const bool ok = session_->map2d_export_bmp(dump, 1280, 720);
      std::fprintf(stderr,
                   "map2d-dump-bmp(post-show): %s path=%s layout=%llu\n",
                   ok ? "ok" : "fail", dump,
                   static_cast<unsigned long long>(
                       session_->map2d_layout_build_count()));
    }
  }
  navigation_baselined_ = true;
  refresh_scale();
  schedule_deferred_china_seed();
}

void Browser::finish_deferred_shell_wiring() {
  if (ui_) {
    ui_->finish_deferred_shell_wiring();
  }
}

int Browser::run_loop() {
  return ui_ ? ui_->run_shell_loop() : 1;
}

void Browser::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  // Cancel deferred product seed before it blocks the UI thread on land-clip
  // (WM_CLOSE cannot run until that timer callback returns).
  cancel_deferred_china_seed();
  // Detach DrawHost / join Display before abandon_mesh. Reversing that
  // order lets Scene3d present hold present_mu_ while the UI thread blocks in
  // abandon, and release_rhi_device waits forever for a destroy ack the Display
  // thread cannot process (close hang).
  plugin_preview_.close();
  if (ui_) {
    ui_->prepare_shell_close();
  }
  session_->prepare_close();
}

HWND Browser::hwnd() const {
  return ui_ ? ui_->hwnd() : nullptr;
}

void Browser::invalidate_map_overlays() {
  if (ui_) {
    ui_->invalidate_map_overlays();
  }
}

ui::views::View* Browser::contents_view() const {
  return ui_ ? ui_->contents_view() : nullptr;
}

ui::views::CatalogView* Browser::catalog_view() const {
  return ui_ ? ui_->catalog_view() : nullptr;
}

ui::views::AmboxView* Browser::ambox_view() const {
  return ui_ ? ui_->ambox_view() : nullptr;
}

ui::views::StatusBar* Browser::status_bar() const {
  return ui_ ? ui_->status_bar() : nullptr;
}

ui::views::FeatureInfo* Browser::feature_info() const {
  return ui_ ? ui_->feature_info() : nullptr;
}

ui::views::AttributeTable* Browser::attribute_table() const {
  return ui_ ? ui_->attribute_table() : nullptr;
}

ui::views::ProcessingPanel* Browser::processing_panel() const {
  return ui_ ? ui_->processing_panel() : nullptr;
}

ui::views::DrawHost* Browser::draw_host() const {
  return ui_ ? ui_->draw_host() : nullptr;
}

ui::views::DrawHost* Browser::data_draw_host() const {
  return ui_ ? ui_->data_draw_host() : nullptr;
}

ui::views::DrawHost* Browser::scene_draw_host() const {
  return ui_ ? ui_->scene_draw_host() : nullptr;
}

content::ToolSession* Browser::edit_tool_session() const {
  return session_->edit_tool_session();
}

void Browser::refit_active_view() {
  fit_map_extent();
}

void Browser::refresh_inspectors() {
  if (ui_) {
    ui_->sync_inspectors_from_scene();
  }
}

void Browser::sync_catalog_from_scene() {
  if (ui_) {
    ui_->sync_catalog_from_scene();
  }
}

void Browser::select_view_tab(int index) {
  if (!ui_) {
    return;
  }
  // Product horizon is Map=0, 3D=1. Legacy Scene index 2 still selects 3D.
  if (index >= 2) {
    index = 1;
  }
  if (index < 0) {
    index = 0;
  }
  ui_->select_view_tab(index);
}

void Browser::OnExtentChanged(uint32_t /*view_id*/, const content::Extent2& e) {
  if (syncing_extent_ || !should_sync_orbit_extent(e)) {
    return;
  }
  // GisContents recv / renderer_recv threads deliver this off the UI thread.
  // OrbitFrame + StatusBar/Label must not run there (heap corruption /
  // 0xC0000374). Always PostMessage — never apply on the caller thread
  // (including the old "no HWND yet" fallback, which still raced Label).
  HWND shell = hwnd();
  if (!shell || !IsWindow(shell)) {
    return;
  }
  auto* heap = new content::Extent2(e);
  constexpr UINT kExtentChangedUi = WM_APP + 0x5253;  // 'RS'
  if (!PostMessageW(shell, kExtentChangedUi, 0,
                    reinterpret_cast<LPARAM>(heap))) {
    delete heap;
  }
}

void Browser::apply_extent_changed_on_ui(const content::Extent2& e) {
  if (syncing_extent_ || !should_sync_orbit_extent(e)) {
    return;
  }
  syncing_extent_ = true;
  session_->apply_orbit_world_extent(e);
  syncing_extent_ = false;
  if (ui_) {
    refresh_scale();
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
