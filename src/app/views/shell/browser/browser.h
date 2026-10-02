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

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/map_session.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
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

using content::MapScene;
using content::ViewFrame;
using content::OrbitFrame;
using content::ViewNavigation;
using content::ViewBookmark;
using content::Map2dPresenter;
using content::Scene3dPresenter;
using content::Scene3dStereoSession;
using content::BlitFrameCache;
using content::MapHwndGestures;
using content::format_view_scale;
using content::kChinaLonLatExtent;
using content::kScene3dDefaultYaw;
using content::Scene3dEngine;
using content::prefer_scene3d_flycube;
using content::prefer_scene3d_gdi;
using content::prefer_scene3d_stereo_gl;
using content::scene3d_engine;
using content::set_scene3d_engine;
using content::extent_looks_like_china;

class PluginShell;

// Chrome controller: owns PluginShell + BrowserUiDelegate. Map document /
// camera / present / gestures live on content::MapSession (WebContents-ish).
class Browser : public content::MapContentsObserver {
 public:
  Browser();
  ~Browser() override;

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  bool init();
  // Product plugin resource root (--plugins-dir). Empty → <exe>/../plugins.
  void set_plugins_dir(std::string path);
  // When true, SeedDocument skips China/DEM OGR open; Browser::show schedules
  // an idle open so first-show chrome stays interactive. Harness paths keep
  // false (sync) so BMP / self-test timing stays deterministic.
  // Out-of-line: parallel ninja + Browser layout churn must not skew callers.
  void set_defer_china_seed(bool defer);
  bool defer_china_seed() const;
  // Opt-in OOP GPU at Session.init_hosts (also SMT_ENABLE_OOP_RENDER=1).
  void set_enable_oop_render(bool enable);
  bool enable_oop_render() const;
  void show();
  int run_loop();

  // Tear down map HWND / FlyCube before Widget DestroyWindow.
  void prepare_close();
  bool is_close_prepared() const { return prepare_close_done_; }

  BrowserUiDelegate* ui() { return ui_.get(); }
  const BrowserUiDelegate* ui() const { return ui_.get(); }

  content::MapSession& session() { return session_; }
  const content::MapSession& session() const { return session_; }

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

  content::MapScene* document() { return &session_.document(); }
  const content::MapScene* document() const { return &session_.document(); }
  content::Scene3dPresenter* scene3d() { return &session_.scene3d(); }
  const content::Scene3dPresenter* scene3d() const {
    return &session_.scene3d();
  }
  content::ViewFrame* view_frame() { return &session_.view_frame(); }
  const content::ViewFrame* view_frame() const {
    return &session_.view_frame();
  }
  content::OrbitFrame* orbit_frame() { return &session_.orbit_frame(); }
  const content::OrbitFrame* orbit_frame() const {
    return &session_.orbit_frame();
  }
  content::Map2dPresenter* map2d() { return &session_.map2d(); }
  const content::Map2dPresenter* map2d() const { return &session_.map2d(); }
  content::Scene3dStereoSession* scene3d_stereo() {
    return &session_.scene3d_stereo();
  }
  content::BlitFrameCache* blit() { return &session_.blit(); }
  content::ViewNavigation* navigation() { return &session_.navigation(); }
  const content::ViewNavigation* navigation() const {
    return &session_.navigation();
  }
  content::MapContents* map_session() { return session_.map_contents(); }
  PluginShell* plugins() { return plugins_.get(); }
  AnalysisPlayback& analysis_playback() { return analysis_playback_; }
  const AnalysisPlayback& analysis_playback() const { return analysis_playback_; }

  // ResultPlayback: show frame |index| on map2d (+ scene stand-in).
  bool apply_analysis_frame(int index);
  // Writes captures/<dir>/frame_XXXX.bmp + playback.json. Returns frame count.
  int export_analysis_frames(const std::string& dir_leaf);

  content::ViewHost* edit_host() { return session_.edit_host(); }
  content::ViewHost* data_host() { return session_.data_host(); }
  content::ViewHost* scene_host() { return session_.scene_host(); }
  content::MapHwndGestures* edit_gestures() {
    return &session_.edit_gestures();
  }
  content::MapHwndGestures* data_gestures() {
    return &session_.data_gestures();
  }
  content::MapHwndGestures* scene_gestures() {
    return &session_.scene_gestures();
  }

  bool syncing_extent() const { return syncing_extent_; }
  void set_syncing_extent(bool v) { syncing_extent_ = v; }
  bool navigation_baselined() const { return navigation_baselined_; }
  void set_navigation_baselined(bool v) { navigation_baselined_ = v; }
  bool flash_lit() const { return flash_lit_; }
  void set_flash_lit(bool v) { flash_lit_ = v; }
  bool extent_watch_open() const { return extent_watch_open_; }
  void set_extent_watch_open(bool v) { extent_watch_open_ = v; }
  content::Extent2* extent_watch() { return &extent_watch_; }
  content::EventBus::Connection* selection_sub();
  content::EventBus::Connection* edit_sub();
  content::EventBus::Connection* extent_sub();

  bool run_tool_command(std::string_view command_id);
  void refit_active_view();
  void refresh_inspectors();
  // Non-inline: forwards to ui_->sync_catalog_from_scene(). Showcase / harness
  // TUs must not call the inline ui() accessor after OGR replace — a skewed
  // Browser layout (stale .obj under parallel ninja) loads freefill into the
  // BrowserUiDelegate* and AVs on the vtable (bug #10 catalog residual).
  void sync_catalog_from_scene();
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

  // MapContentsObserver ? extent sync into ViewFrame / OrbitFrame.
  void OnExtentChanged(uint32_t view_id, const content::Extent2& e) override;

 private:
  content::MapSession session_;
  AnalysisPlayback analysis_playback_;
  std::unique_ptr<PluginShell> plugins_;
  std::string plugins_dir_;

  content::EventBus::Connection selection_sub_;
  content::EventBus::Connection edit_sub_;
  content::EventBus::Connection extent_sub_;

  content::Extent2 extent_watch_{};
  bool navigation_baselined_ = false;
  bool extent_watch_open_ = false;
  bool syncing_extent_ = false;
  bool prepare_close_done_ = false;
  bool flash_lit_ = true;

  // Append-only flags: keep ahead of ui_ only. Inserting before Connection
  // members skews stale shell_ui .obj (parallel ninja) so selection_sub()
  // lands on CD-fill → AV in EventBus::Connection::disconnect.
  bool defer_china_seed_ = false;
  bool enable_oop_render_ = false;

  // Declared last so chrome tears down before session members.
  std::unique_ptr<BrowserUiDelegate> ui_;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_BROWSER_BROWSER_H_
