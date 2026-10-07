// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_SHELL_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_SHELL_H_

#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/browser/session/browser_session.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"

namespace content {
class MapContents;
class PluginHost;
}  // namespace content

namespace ui {
namespace views {
class CatalogView;
class DrawHost;
class FeatureInfo;
class StatusBar;
class View;
}  // namespace views
}  // namespace ui

namespace plugin {

inline constexpr std::string_view kCapabilityHarness = "app.views.harness";

// Chrome publishes a Browser adapter under kCapabilityHarness. Product
// scenario TUs depend on this vtable, not on app::Browser.
class HarnessShell {
 public:
  virtual ~HarnessShell() = default;

  virtual HWND hwnd() const = 0;
  virtual ui::views::View* contents_view() const = 0;
  virtual ui::views::CatalogView* catalog_view() const = 0;
  virtual ui::views::StatusBar* status_bar() const = 0;
  virtual ui::views::FeatureInfo* feature_info() const = 0;
  virtual ui::views::DrawHost* draw_host() const = 0;
  virtual ui::views::DrawHost* data_draw_host() const = 0;
  virtual ui::views::DrawHost* scene_draw_host() const = 0;
  virtual content::ViewHost* edit_view_host() const = 0;
  virtual content::ViewHost* scene_host() = 0;
  virtual content::MapScene* document() = 0;
  virtual content::Scene3dPresenter* scene3d() = 0;
  virtual content::Scene3dStereoSession* scene3d_stereo() = 0;
  virtual content::ViewFrame* view_frame() = 0;
  virtual content::OrbitFrame* orbit_frame() = 0;
  virtual content::Map2dPresenter* map2d() = 0;
  virtual content::MapContents* map_contents() = 0;
  virtual content::PluginHost* plugin_host() = 0;

  virtual bool run_tool_command(std::string_view command_id) = 0;
  virtual void refit_active_view() = 0;
  virtual void refresh_inspectors() = 0;
  virtual bool run_m2_harness_hooks(std::string* err) = 0;
  virtual void select_map_tab(int index) = 0;
  virtual void on_view_command(std::string_view command_id, int bookmark_index,
                               bool from_context, int view_x, int view_y) = 0;
  virtual void fit_map_extent() = 0;

  virtual void pump(DWORD ms) = 0;
  virtual void mark(const char* step) = 0;
  virtual void mark_named(const wchar_t* leaf, const char* step,
                          bool truncate) = 0;
  virtual void detach_maps() = 0;
  virtual void finish_scene3d(bool borrowed_shell) = 0;
  virtual void stop_present_timers() = 0;
  virtual void resume_present_timers() = 0;
  virtual void push_shared_extent() = 0;
  virtual bool capture_path(wchar_t* out, size_t cap, const wchar_t* leaf) = 0;
  virtual bool capture_path_a(char* out, size_t cap, const char* leaf) = 0;
  virtual bool exe_dir_slash(wchar_t* out, size_t cap) = 0;
  virtual bool bmp_has_visible_signal(const char* path_a, int* out_w,
                                      int* out_h) = 0;

  // Host-owned chrome: processing playback + modeless plugin preview.
  virtual bool apply_plugin_frame(int index) = 0;
  virtual bool preview_is_open() const = 0;
  virtual void preview_close() = 0;
  virtual bool preview_export_bmp(const char* path_utf8) = 0;
};

inline HarnessShell* harness_shell(content::PluginHost* host) {
  return host ? static_cast<HarnessShell*>(
                    host->query_capability(kCapabilityHarness))
              : nullptr;
}

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_SHELL_H_
