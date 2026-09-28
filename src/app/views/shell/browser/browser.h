// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_BROWSER_BROWSER_H_
#define APP_VIEWS_SHELL_BROWSER_BROWSER_H_

#include <memory>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/camera/orbit_frame.h"
#include "app/views/camera/view_frame.h"
#include "app/views/camera/view_navigation.h"
#include "app/views/document/map_scene.h"
#include "app/views/input/map_hwnd_gestures.h"
#include "app/views/present/host/blit_frame_cache.h"
#include "app/views/present/map2d/map2d_presenter.h"
#include "app/views/present/scene3d/scene3d_presenter.h"
#include "app/views/present/scene3d/session/scene3d_stereo_session.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "content/public/event_bus.h"
#include "content/public/map_contents_observer.h"
#include "content/public/map_types.h"
#include "tool/draft/draft.h"

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
class ProcessingPanel;
class StatusBar;
class View;
}  // namespace views
}  // namespace ui

namespace app {

class PluginShell;

// Process-root controller for SmartGisViews: owns the map session and
// presenters. Chrome is owned via BrowserUiDelegate (BrowserView).
class Browser : public content::MapContentsObserver {
 public:
  Browser();
  ~Browser() override;

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  bool init();
  void show();
  int run_loop();

  // Tear down map HWND / FlyCube before Widget DestroyWindow.
  void prepare_close();

  BrowserUiDelegate* ui() { return ui_.get(); }
  const BrowserUiDelegate* ui() const { return ui_.get(); }

  // UI forwards (self-test / showcase).
  HWND hwnd() const;
  ui::views::View* contents_view() const;
  ui::views::CatalogView* catalog_view() const;
  ui::views::AmboxView* ambox_view() const;
  ui::views::StatusBar* status_bar() const;
  ui::views::FeatureInfo* feature_info() const;
  ui::views::AttributeTable* attribute_table() const;
  ui::views::ProcessingPanel* processing_panel() const;
  ui::views::MapViewport* map_viewport() const;
  ui::views::MapViewport* map_data_viewport() const;
  ui::views::MapViewport* map_scene_viewport() const;
  content::ViewHost* edit_view_host() const;

  MapScene* document() { return &document_; }
  const MapScene* document() const { return &document_; }
  Scene3dPresenter* scene3d() { return &scene3d_; }
  const Scene3dPresenter* scene3d() const { return &scene3d_; }
  ViewFrame* view_frame() { return &view_frame_; }
  const ViewFrame* view_frame() const { return &view_frame_; }
  OrbitFrame* orbit_frame() { return &orbit_; }
  const OrbitFrame* orbit_frame() const { return &orbit_; }
  Map2dPresenter* map2d() { return &map2d_; }
  const Map2dPresenter* map2d() const { return &map2d_; }
  Scene3dStereoSession* scene3d_stereo() { return &scene3d_stereo_; }
  BlitFrameCache* blit() { return &blit_; }
  ViewNavigation* navigation() { return &navigation_; }
  const ViewNavigation* navigation() const { return &navigation_; }
  content::MapContents* map_session() { return map_session_.get(); }
  PluginShell* plugins() { return plugins_.get(); }
  content::ViewHost* edit_host() { return edit_host_.get(); }
  content::ViewHost* data_host() { return data_host_.get(); }
  content::ViewHost* scene_host() { return scene_host_.get(); }
  MapHwndGestures* edit_gestures() { return &edit_gestures_; }
  MapHwndGestures* data_gestures() { return &data_gestures_; }
  MapHwndGestures* scene_gestures() { return &scene_gestures_; }

  bool syncing_extent() const { return syncing_extent_; }
  void set_syncing_extent(bool v) { syncing_extent_ = v; }
  bool navigation_baselined() const { return navigation_baselined_; }
  void set_navigation_baselined(bool v) { navigation_baselined_ = v; }
  bool flash_lit() const { return flash_lit_; }
  void set_flash_lit(bool v) { flash_lit_ = v; }
  bool extent_watch_open() const { return extent_watch_open_; }
  void set_extent_watch_open(bool v) { extent_watch_open_ = v; }
  content::Extent2* extent_watch() { return &extent_watch_; }
  content::EventBus::Connection* selection_sub() { return &selection_sub_; }
  content::EventBus::Connection* edit_sub() { return &edit_sub_; }
  content::EventBus::Connection* extent_sub() { return &extent_sub_; }

  bool run_tool_command(std::string_view command_id);
  void refit_active_view();
  void refresh_inspectors();
  bool apply_atmosphere_fields(std::string_view spec);
  bool run_m2_self_test_hooks(std::string* err);
  void select_map_tab(int index);

  void on_catalog_command(const std::string& command_id);
  void on_open();
  void on_save_document();
  void on_export_document();
  void on_view_command(std::string_view command_id, int bookmark_index,
                       bool from_context, int view_x, int view_y);
  bool dispatch_shell_navigation(std::string_view command_id,
                                 int bookmark_index, bool from_context,
                                 int view_x, int view_y);

  void apply_nav_draft(const tool::Draft& draft, bool pan, double zoom_factor);
  void zoom_at_and_commit(int view_x, int view_y, double factor);
  void fit_map_extent();
  void handle_draft(const tool::Draft& draft);
  void refresh_scale();
  void adopt_or_commit_extent();
  void on_extent_watch(bool begin);
  void frame_navigation_extent();
  void identify_at(int view_x, int view_y);
  void forward_draft_to_contents(const tool::Draft& draft);
  void commit_blit_preview();
  void push_shared_extent();
  void handle_pinch(int view_x, int view_y, double scale);
  void handle_gesture_pan(int dx_px, int dy_px);
  void pull_orbit_extent();

  // MapContentsObserver — extent sync into ViewFrame / OrbitFrame.
  void OnExtentChanged(uint32_t view_id, const content::Extent2& e) override;

 private:
  // Session / present / input — owned here, not by chrome.
  MapScene document_;
  ViewFrame view_frame_;
  OrbitFrame orbit_;
  Map2dPresenter map2d_;
  Scene3dPresenter scene3d_;
  Scene3dStereoSession scene3d_stereo_;
  std::unique_ptr<content::ViewHost> edit_host_;
  std::unique_ptr<content::ViewHost> data_host_;
  std::unique_ptr<content::ViewHost> scene_host_;
  std::unique_ptr<content::MapContents> map_session_;
  std::unique_ptr<PluginShell> plugins_;

  content::EventBus::Connection selection_sub_;
  content::EventBus::Connection edit_sub_;
  content::EventBus::Connection extent_sub_;

  MapHwndGestures edit_gestures_;
  MapHwndGestures data_gestures_;
  MapHwndGestures scene_gestures_;
  BlitFrameCache blit_;
  ViewNavigation navigation_;
  content::Extent2 extent_watch_{};
  bool navigation_baselined_ = false;
  bool extent_watch_open_ = false;
  bool syncing_extent_ = false;
  bool prepare_close_done_ = false;
  bool flash_lit_ = true;

  // Declared last so chrome tears down before session members.
  std::unique_ptr<BrowserUiDelegate> ui_;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_BROWSER_H_
