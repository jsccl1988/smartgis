// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_BROWSER_VIEW_H_
#define APP_VIEWS_SHELL_UI_BROWSER_VIEW_H_

#include <functional>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "ui/views/kernel/widget/widget.h"

namespace content {
class ViewHost;
}  // namespace content

namespace ui {
namespace views {
class AmboxView;
class AtmospherePanel;
class AttributeTable;
class CatalogView;
class FeatureInfo;
class MapViewport;
class MenuBar;
class ProcessingPanel;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace app {

class Browser;
class MapHwndGestures;

// Views chrome for SmartGisViews: MenuBar, splitters, TabStrip, map panes.
// Session / present ownership lives on Browser; this type holds Browser* and
// implements BrowserUiDelegate for status/tab UI push.
class BrowserView : public BrowserUiDelegate {
 public:
  explicit BrowserView(Browser* browser);
  ~BrowserView() override;

  BrowserView(const BrowserView&) = delete;
  BrowserView& operator=(const BrowserView&) = delete;

  Browser* browser() const { return browser_; }

  // BrowserUiDelegate
  bool init_chrome() override;
  void show_chrome() override;
  int run_chrome_loop() override;
  void prepare_chrome_close() override;

  HWND hwnd() const override;
  ui::views::View* contents_view() const override;
  ui::views::CatalogView* catalog_view() const override { return catalog_; }
  ui::views::AmboxView* ambox_view() const override { return ambox_; }
  ui::views::StatusBar* status_bar() const override { return status_bar_; }
  ui::views::FeatureInfo* feature_info() const override {
    return feature_info_;
  }
  ui::views::AttributeTable* attribute_table() const override {
    return attribute_table_;
  }
  ui::views::ProcessingPanel* processing_panel() const override {
    return processing_panel_;
  }
  ui::views::AtmospherePanel* atmosphere_panel() const override {
    return atmosphere_panel_;
  }
  ui::views::TabStrip* inspector_tabs() const override {
    return inspector_tabs_;
  }
  ui::views::MapViewport* map_viewport() const override { return map_edit_; }
  ui::views::MapViewport* map_data_viewport() const override {
    return map_data_;
  }
  ui::views::MapViewport* map_scene_viewport() const override {
    return map_scene_;
  }

  ui::views::MapViewport* active_map() const override;
  content::ViewHost* active_view_host() const override;
  void active_view_size(int* w, int* h) const override;
  bool scene3d_tab_active() const override;

  void set_status_message(const std::string& text) override;
  void invalidate_map_overlays() override;
  void sync_catalog_from_scene() override;
  void sync_inspectors_from_scene() override;
  void sync_flash_timer() override;
  void schedule_menu_rebuild() override;
  void schedule_overlay_full_redraw() override;
  void select_map_tab(int index) override;
  void show_feature_info_tab() override;
  void sync_status() override;
  void populate_ambox() override;
  void for_each_map_viewport(
      const std::function<void(ui::views::MapViewport*)>& fn) const override;

 private:
  void build_contents();
  void attach_viewports();
  void wire_catalog();
  void wire_edit_feedback();
  void wire_map_scene();
  void wire_atmosphere_panel();
  void wire_processing_panel();
  void commit_widget_shell_to_maps();
  void commit_widget_shell_to_maps(const ui::views::Rect& dirty);
  void attach_hwnd_gestures();
  void configure_gestures(MapHwndGestures* gestures);
  void rebuild_menus();
  void on_map_right_click(HWND hwnd, int view_x, int view_y);
  void on_exit();
  void on_plugins();
  void on_processing();
  void run_processing_operator(const std::string& processing_id);
  void switch_map_tab(int i);
  void wire_tool_seams();

  Browser* browser_ = nullptr;

  ui::views::Widget widget_;
  ui::views::CatalogView* catalog_ = nullptr;
  ui::views::AmboxView* ambox_ = nullptr;
  ui::views::FeatureInfo* feature_info_ = nullptr;
  ui::views::AttributeTable* attribute_table_ = nullptr;
  ui::views::AtmospherePanel* atmosphere_panel_ = nullptr;
  ui::views::ProcessingPanel* processing_panel_ = nullptr;
  ui::views::TabStrip* inspector_tabs_ = nullptr;
  ui::views::MapViewport* map_edit_ = nullptr;
  ui::views::MapViewport* map_data_ = nullptr;
  ui::views::MapViewport* map_scene_ = nullptr;
  ui::views::TabStrip* map_tabs_ = nullptr;
  ui::views::MenuBar* menu_bar_ = nullptr;
  ui::views::StatusBar* status_bar_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_BROWSER_VIEW_H_
