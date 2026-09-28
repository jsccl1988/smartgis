// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_BROWSER_UI_DELEGATE_H_
#define APP_VIEWS_SHELL_BROWSER_BROWSER_UI_DELEGATE_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
class ViewHost;
}  // namespace content

namespace tool {
struct Draft;
}  // namespace tool

namespace ui {
namespace views {
class AmboxView;
class AtmospherePanel;
class AttributeTable;
class CatalogView;
class FeatureInfo;
class MapViewport;
class ProcessingPanel;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace app {

class Browser;

// Thin chrome surface for Browser → UI notifications (Chromium BrowserWindow
// shape). Implemented by BrowserView in shell/ui; browser/ must not include
// concrete BrowserView.
class BrowserUiDelegate {
 public:
  virtual ~BrowserUiDelegate() = default;

  virtual bool init_chrome() = 0;
  virtual void show_chrome() = 0;
  virtual int run_chrome_loop() = 0;
  virtual void prepare_chrome_close() = 0;

  virtual HWND hwnd() const = 0;
  virtual ui::views::View* contents_view() const = 0;
  virtual ui::views::CatalogView* catalog_view() const = 0;
  virtual ui::views::AmboxView* ambox_view() const = 0;
  virtual ui::views::StatusBar* status_bar() const = 0;
  virtual ui::views::FeatureInfo* feature_info() const = 0;
  virtual ui::views::AttributeTable* attribute_table() const = 0;
  virtual ui::views::ProcessingPanel* processing_panel() const = 0;
  virtual ui::views::AtmospherePanel* atmosphere_panel() const = 0;
  virtual ui::views::TabStrip* inspector_tabs() const = 0;
  virtual ui::views::MapViewport* map_viewport() const = 0;
  virtual ui::views::MapViewport* map_data_viewport() const = 0;
  virtual ui::views::MapViewport* map_scene_viewport() const = 0;

  virtual ui::views::MapViewport* active_map() const = 0;
  virtual content::ViewHost* active_view_host() const = 0;
  virtual void active_view_size(int* w, int* h) const = 0;
  virtual bool scene3d_tab_active() const = 0;

  // When Measure panel is armed, consume draw drafts as measurements (no edit).
  virtual bool try_consume_measure_draft(const tool::Draft& draft) = 0;

  virtual void set_status_message(const std::string& text) = 0;
  virtual void invalidate_map_overlays() = 0;
  virtual void sync_catalog_from_scene() = 0;
  virtual void sync_inspectors_from_scene() = 0;
  virtual void sync_flash_timer() = 0;
  virtual void schedule_menu_rebuild() = 0;
  virtual void schedule_overlay_full_redraw() = 0;
  virtual void select_map_tab(int index) = 0;
  virtual void show_feature_info_tab() = 0;
  virtual void sync_status() = 0;
  virtual void populate_ambox() = 0;
  virtual void for_each_map_viewport(
      const std::function<void(ui::views::MapViewport*)>& fn) const = 0;
};

// Defined in shell/ui (BrowserView). Keeps browser.cc free of concrete UI.
std::unique_ptr<BrowserUiDelegate> create_browser_ui(Browser* browser);

}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_BROWSER_UI_DELEGATE_H_
