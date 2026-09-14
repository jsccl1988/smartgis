// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_VIEW_H_
#define APP_VIEWS_BROWSER_VIEW_H_

#include <memory>
#include <string>
#include <string_view>

#include "content/public/event_bus.h"
#include "ui/views/widget.h"

namespace content {
class MapContents;
class ViewHost;
}  // namespace content

namespace ui {
namespace views {
class AmboxView;
class AttributeTable;
class CatalogView;
class FeatureInfo;
class MapViewport;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace app {

class PluginChrome;

// Product chrome: single-window IDE layout (MenuBar, splitters, TabStrip).
// Business panels are composed, never painted by this host.
class BrowserView {
 public:
  BrowserView();
  ~BrowserView();

  BrowserView(const BrowserView&) = delete;
  BrowserView& operator=(const BrowserView&) = delete;

  bool init();
  void show();
  int run_loop();

  HWND hwnd() const;
  ui::views::View* contents_view() const;
  ui::views::CatalogView* catalog_view() const { return catalog_; }
  ui::views::AmboxView* ambox_view() const { return ambox_; }
  ui::views::StatusBar* status_bar() const { return status_bar_; }
  // Map Edit host: select / draw / EditSession path used by --self-test.
  content::ViewHost* edit_view_host() const { return edit_host_.get(); }
  // Map Edit (2D) pane: primary hang used by --self-test / wait_ready.
  ui::views::MapViewport* map_viewport() const { return map_edit_; }
  ui::views::MapViewport* map_data_viewport() const { return map_data_; }
  ui::views::MapViewport* map_scene_viewport() const { return map_scene_; }
  // Switch Map | Data | 3D tabs (0/1/2). Used by --self-test for 3D cover.
  void select_map_tab(int index) { switch_map_tab(index); }

  // Activate or fire a Workspace / chrome tool id; updates the status bar.
  // Aliases: select|identify → selection.point, pan → view.pan.
  bool run_tool_command(std::string_view command_id);

 private:
  void build_contents();
  void attach_viewports();
  void wire_catalog();
  void wire_edit_feedback();
  // Fill Ambox from Workspace + PluginHost CommandCatalogs (id-prefix groups).
  void populate_ambox();
  void on_catalog_command(const std::string& command_id);
  void on_open();
  void on_exit();
  void on_plugins();
  void switch_map_tab(int i);
  void sync_status();
  void set_status_message(const std::string& text);
  ui::views::MapViewport* active_map() const;
  content::ViewHost* active_view_host() const;

  std::unique_ptr<content::ViewHost> edit_host_;
  std::unique_ptr<content::ViewHost> data_host_;
  std::unique_ptr<content::ViewHost> scene_host_;
  std::unique_ptr<content::MapContents> map_session_;
  std::unique_ptr<PluginChrome> plugins_;

  content::EventBus::Connection selection_sub_;
  content::EventBus::Connection edit_sub_;

  ui::views::Widget widget_;
  ui::views::CatalogView* catalog_ = nullptr;
  ui::views::AmboxView* ambox_ = nullptr;
  ui::views::FeatureInfo* feature_info_ = nullptr;
  ui::views::AttributeTable* attribute_table_ = nullptr;
  ui::views::MapViewport* map_edit_ = nullptr;
  ui::views::MapViewport* map_data_ = nullptr;
  ui::views::MapViewport* map_scene_ = nullptr;
  ui::views::TabStrip* map_tabs_ = nullptr;
  ui::views::StatusBar* status_bar_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_BROWSER_VIEW_H_
