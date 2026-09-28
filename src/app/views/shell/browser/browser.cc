// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <cstring>

#include "app/views/camera/map_host_extent.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "plugin/product/dem/dem_commands.h"

namespace app {

Browser::Browser() = default;

Browser::~Browser() {
  prepare_close();
  if (map_session_) {
    map_session_->SetObserver(nullptr);
  }
  if (plugins_) {
    plugins_->shutdown();
    plugins_.reset();
  }
  ui_.reset();
}

bool Browser::init() {
  edit_host_ = std::make_unique<content::ViewHost>();
  data_host_ = std::make_unique<content::ViewHost>();
  scene_host_ = std::make_unique<content::ViewHost>();
  map_session_.reset(content::MapContents::Create());
  if (map_session_ && !map_session_->StartRenderProcess()) {
    map_session_.reset();
  }

  plugins_ = std::make_unique<PluginShell>();
  if (!plugins_->init(edit_host_->events())) {
    plugins_.reset();
    return false;
  }

  ui_ = create_browser_ui(this);
  if (!ui_) {
    return false;
  }

  plugin::set_dem_surface_writer(
      [this](const double* xyz, int point_count, const int* triangles,
             int triangle_count, const char* op) {
        const char* name =
            (op && std::strstr(op, "grid")) ? "DEM grid" : "DEM tin";
        if (!document_.add_triangle_layer(name, xyz, point_count, triangles,
                                          triangle_count)) {
          return false;
        }
        if (ui_) {
          ui_->sync_catalog_from_scene();
          ui_->invalidate_map_overlays();
        }
        return true;
      });

  return ui_->init_chrome();
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
  edit_gestures_.detach();
  data_gestures_.detach();
  scene_gestures_.detach();
  scene3d_.abandon_mesh();
  scene3d_stereo_.release();
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
  return edit_host_.get();
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
  int w = 800;
  int h = 600;
  if (ui_) {
    ui_->active_view_size(&w, &h);
  }
  syncing_extent_ = true;
  view_frame_.apply_world_extent(e, w, h);
  orbit_.apply_world_extent(e);
  syncing_extent_ = false;
  if (ui_) {
    refresh_scale();
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
