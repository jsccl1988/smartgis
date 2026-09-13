// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_VIEW_H_
#define APP_VIEWS_BROWSER_VIEW_H_

#include <memory>
#include <string>

#include "ui/views/widget.h"

namespace content {
class MapContents;
class ViewHost;
}  // namespace content

namespace ui {
namespace views {
class CatalogView;
class MapViewport;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace app {

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
  // Map Edit (2D) pane: primary hang used by --self-test / wait_ready.
  ui::views::MapViewport* map_viewport() const { return map_edit_; }

 private:
  void build_contents();
  void attach_viewports();
  void wire_catalog();
  void on_catalog_command(const std::string& command_id);
  void on_open();
  void on_exit();
  void switch_map_tab(int i);
  void sync_status();
  ui::views::MapViewport* active_map() const;
  content::ViewHost* active_view_host() const;

  std::unique_ptr<content::ViewHost> edit_host_;
  std::unique_ptr<content::ViewHost> data_host_;
  std::unique_ptr<content::ViewHost> scene_host_;
  std::unique_ptr<content::MapContents> map_session_;

  ui::views::Widget widget_;
  ui::views::CatalogView* catalog_ = nullptr;
  ui::views::MapViewport* map_edit_ = nullptr;
  ui::views::MapViewport* map_data_ = nullptr;
  ui::views::MapViewport* map_scene_ = nullptr;
  ui::views::TabStrip* map_tabs_ = nullptr;
  ui::views::StatusBar* status_bar_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_BROWSER_VIEW_H_
