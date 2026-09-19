// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_BROWSER_VIEW_H_
#define APP_VIEWS_BROWSER_VIEW_H_

#include <memory>
#include <string>
#include <string_view>

#include "app/views/blit_frame_cache.h"
#include "app/views/map_hwnd_gestures.h"
#include "app/views/map_scene.h"
#include "app/views/scene3d_controller.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents_observer.h"
#include "content/public/map_types.h"
#include "tool/gestures.h"
#include "ui/views/kernel/widget.h"

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
// Map panes host the shared MapContents scene (same leftover SmartGis session).
class BrowserView : public content::MapContentsObserver {
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

  // In-process layers + features (Catalog / overlay paint / edit).
  MapScene* document() { return &document_; }
  const MapScene* document() const { return &document_; }
  Scene3dController* scene3d() { return &scene3d_; }
  const Scene3dController* scene3d() const { return &scene3d_; }

  // Activate or fire a Workspace / chrome tool id; updates the status bar.
  // Aliases: select|identify 鈫?selection.point, pan 鈫?view.pan.
  bool run_tool_command(std::string_view command_id);

 private:
  void build_contents();
  void attach_viewports();
  void wire_catalog();
  void wire_edit_feedback();
  void wire_map_scene();
  void sync_catalog_from_scene();
  void sync_inspectors_from_scene();
  void invalidate_map_overlays();
  // Fit document extent into the active map HWND (Catalog View / view.full).
  void fit_map_extent();
  void handle_draft(const tool::Draft& draft);
  void OnExtentChanged(uint32_t view_id, const content::Extent2& e) override;
  void push_shared_extent();
  void forward_draft_to_contents(const tool::Draft& draft);
  void attach_hwnd_gestures();
  void handle_pinch(int view_x, int view_y, double scale);
  void active_view_size(int* w, int* h) const;
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

  MapScene document_;
  Scene3dController scene3d_;
  std::unique_ptr<content::ViewHost> edit_host_;
  std::unique_ptr<content::ViewHost> data_host_;
  std::unique_ptr<content::ViewHost> scene_host_;
  std::unique_ptr<content::MapContents> map_session_;
  std::unique_ptr<PluginChrome> plugins_;

  content::EventBus::Connection selection_sub_;
  content::EventBus::Connection edit_sub_;
  content::EventBus::Connection extent_sub_;

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

  MapHwndGestures edit_gestures_;
  MapHwndGestures data_gestures_;
  MapHwndGestures scene_gestures_;
  BlitFrameCache blit_;
  bool syncing_extent_ = false;

  void schedule_overlay_full_redraw();
};

}  // namespace app

#endif  // APP_VIEWS_BROWSER_VIEW_H_
