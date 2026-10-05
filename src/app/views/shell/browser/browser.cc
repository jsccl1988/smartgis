// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

#include "app/views/shell/browser/ui_delegate.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/map_contents.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {

Browser::Browser() : session_(content::BrowserSession::create()) {}

Browser::~Browser() {
  prepare_close();
  session_->clear_map_contents_observer();
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
    BASE_TRACE_EVENT("Session.init_hosts", "startup");
    LOGGING(LOG_INFO, "startup: session.init_hosts");
    session_->init_hosts();
    if (enable_oop_render_) {
      if (!session_->ensure_oop_render_process()) {
        LOGGING(LOG_WARNING,
                "startup: enable_oop_render requested but StartRenderProcess "
                "failed — continuing in-process");
      }
    }
  }

  {
    BASE_TRACE_EVENT("PluginShell.init", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell.init");
    plugins_ = std::make_unique<PluginShell>();
    plugins_->set_plugins_dir(plugins_dir_);
    if (!session_->edit_host() ||
        !plugins_->init(session_->edit_host()->events())) {
      LOGGING(LOG_ERROR, "startup: PluginShell.init failed");
      if (plugins_) {
        plugins_->shutdown();
        plugins_.reset();
      }
      // Showcase / self-test can still paint map2d without plugins; product
      // interactive shell keeps hard-fail by returning false below when UI
      // creation also requires plugins. Soft-continue so chrome can load.
      LOGGING(LOG_WARNING, "startup: continuing without PluginShell");
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
  // Fit after chrome is visible (final client size). Extent-only nudge;
  // init_shell already framed from SeedDocument (demo or sync China).
  // Showcase / harness (SKIP_AMBOX_CATALOG): demo-only seed AVs inside
  // fit_map_extent (cdb world3d-early2 Browser::show). Scene3D framing is
  // applied later by apply_china_scene3d_product_defaults.
  const bool skip_fit = []() {
    const char* skip = base::switch_cstr("skip-ambox-catalog");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  const bool first_carto_ready =
      map2d() && map2d()->layout_build_count() > 0;
  if (!skip_fit && !first_carto_ready) {
    if (!seh_fit_map_extent(this)) {
      LOGGING(LOG_WARNING, "startup: Browser::show fit_map_extent SEH");
    }
  }
  // Sync China seed (default product path): drop any pre-china FlyCube latch
  // so the first interactive present records china carto — same face as
  // --ui-showcase=shell without forcing GDI overlay.
  if (document() && document()->has_china_extent() && !first_carto_ready) {
    if (content::Map2dPresenter* map2d = this->map2d()) {
      map2d->note_surface_reset();
      map2d->invalidate_frame_cache();
    }
    if (ui_) {
      ui_->invalidate_native_map();
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
  // Cancel deferred China seed before it blocks the UI thread on land-clip
  // (WM_CLOSE cannot run until that timer callback returns).
  if (HWND shell = hwnd()) {
    if (IsWindow(shell)) {
      constexpr UINT_PTR kDeferChina = 0x43484E41u;  // 'CHNA'
      KillTimer(shell, kDeferChina);
      RemovePropW(shell, L"DeferChinaBrowser");
    }
  }
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

content::ViewHost* Browser::edit_view_host() const {
  return session_->edit_host();
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

void Browser::select_map_tab(int index) {
  if (!ui_) {
    return;
  }
  // Product chrome is Map=0, 3D=1. Legacy Scene index 2 still selects 3D.
  if (index >= 2) {
    index = 1;
  }
  if (index < 0) {
    index = 0;
  }
  ui_->select_map_tab(index);
}

void Browser::OnExtentChanged(uint32_t /*view_id*/, const content::Extent2& e) {
  if (syncing_extent_ || !extent_nonempty(e)) {
    return;
  }
  // 2D ViewFrame is owned by shell pan/wheel navigation. Applying remote
  // ExtentChanged here races in-flight push_shared_extent echoes and undoes
  // cursor zoom (self-test exit 47). Orbit tracks only a China lon/lat box;
  // pixel or world extents shrink the DEM into a sticker on the ocean.
  if (!extent_looks_like_china(e)) {
    return;
  }
  // MapContents recv / renderer_recv threads deliver this off the UI thread.
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
  if (syncing_extent_ || !extent_nonempty(e) || !extent_looks_like_china(e)) {
    return;
  }
  syncing_extent_ = true;
  session_->orbit_frame().apply_world_extent(e);
  syncing_extent_ = false;
  if (ui_) {
    refresh_scale();
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
