// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writers.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"

namespace app {

Browser::Browser() = default;

Browser::~Browser() {
  prepare_close();
  session_.clear_map_contents_observer();
  if (plugins_) {
    plugins_->shutdown();
    plugins_.reset();
  }
  ui_.reset();
}

void Browser::set_plugins_dir(std::string path) {
  plugins_dir_ = std::move(path);
}

bool Browser::init() {
  BASE_TRACE_EVENT("Browser.init.body", "startup");
  {
    BASE_TRACE_EVENT("Session.init_hosts", "startup");
    LOGGING(LOG_INFO, "startup: session.init_hosts");
    session_.init_hosts();
  }

  {
    BASE_TRACE_EVENT("PluginShell.init", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell.init");
    plugins_ = std::make_unique<PluginShell>();
    plugins_->set_plugins_dir(plugins_dir_);
    if (!session_.edit_host() ||
        !plugins_->init(session_.edit_host()->events())) {
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

  wire_plugin_analysis_writers(this);

  BASE_TRACE_EVENT("InitChrome", "startup");
  LOGGING(LOG_INFO, "startup: init_chrome");
  const bool ok = ui_->init_chrome();
  if (!ok) {
    LOGGING(LOG_ERROR, "startup: init_chrome failed");
  }
  return ok;
}

void Browser::show() {
  if (ui_) {
    ui_->show_chrome();
  }
  fit_map_extent();
  navigation_baselined_ = true;
  refresh_scale();
}

int Browser::run_loop() {
  return ui_ ? ui_->run_chrome_loop() : 1;
}

void Browser::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  session_.prepare_close();
  if (ui_) {
    ui_->prepare_chrome_close();
  }
}

HWND Browser::hwnd() const {
  return ui_ ? ui_->hwnd() : nullptr;
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

ui::views::MapViewport* Browser::map_viewport() const {
  return ui_ ? ui_->map_viewport() : nullptr;
}

ui::views::MapViewport* Browser::map_data_viewport() const {
  return ui_ ? ui_->map_data_viewport() : nullptr;
}

ui::views::MapViewport* Browser::map_scene_viewport() const {
  return ui_ ? ui_->map_scene_viewport() : nullptr;
}

content::ViewHost* Browser::edit_view_host() const {
  return session_.edit_host();
}

void Browser::refit_active_view() {
  fit_map_extent();
}

void Browser::refresh_inspectors() {
  if (ui_) {
    ui_->sync_inspectors_from_scene();
  }
}

void Browser::select_map_tab(int index) {
  if (ui_) {
    ui_->select_map_tab(index);
  }
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
  syncing_extent_ = true;
  session_.orbit_frame().apply_world_extent(e);
  syncing_extent_ = false;
  if (ui_) {
    refresh_scale();
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
