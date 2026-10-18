// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shell operations the IL runtime needs. app/views fills one adapter from
// Browser; this header does not include app/views.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_BIND_SHELL_BRIDGE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_BIND_SHELL_BRIDGE_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/glang.runtime/host/bind/pane.h"

namespace content {
class BlitFrameCache;
class BrowserSession;
class GisContents;
class GisScene;
class Map2dPresenter;
class OrbitFrame;
class PluginHost;
class Scene3dPresenter;
class ToolSession;
class ViewFrame;
}  // namespace content

namespace plugin {
class HarnessShell;
}

namespace ui {
namespace views {
class CatalogView;
class StatusBar;
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace content {

// Browser-shaped access used while binding CapabilityHost and while a suite
// runs. The shell registration owns the only implementation.
class ShellBridge {
 public:
  virtual ~ShellBridge() = default;

  virtual GisScene* document() = 0;
  virtual Map2dPresenter* map2d() = 0;
  virtual Scene3dPresenter* scene3d() = 0;
  virtual ViewFrame* view_frame() = 0;
  virtual OrbitFrame* orbit_frame() = 0;
  virtual BrowserSession& session() = 0;
  virtual HWND hwnd() const = 0;
  virtual Pane* draw_host() = 0;
  virtual Pane* data_draw_host() = 0;
  virtual Pane* scene_draw_host() = 0;
  virtual ToolSession* edit_tool_session() = 0;
  virtual ToolSession* data_tool_session() = 0;
  virtual ToolSession* scene_tool_session() = 0;
  virtual bool run_tool_command(std::string_view command_id) = 0;
  virtual void select_view_tab(int index) = 0;
  virtual void on_view_command(std::string_view command_id,
                               int bookmark_index,
                               bool from_context,
                               int view_x,
                               int view_y) = 0;
  virtual void fit_map_extent() = 0;
  virtual void pump(std::uint32_t ms) = 0;
  virtual ui::views::View* contents_view() const = 0;
  virtual ui::views::CatalogView* catalog_view() = 0;
  virtual ui::views::StatusBar* status_bar() = 0;
  virtual GisContents* gis_contents() = 0;
  virtual BlitFrameCache* blit() = 0;

  virtual bool has_ui() const = 0;
  virtual void invalidate_native_map() = 0;
  virtual void invalidate_map_overlays() = 0;
  virtual void schedule_overlay_full_redraw() = 0;
  virtual void for_each_draw_host(const std::function<void(Pane*)>& fn) = 0;
  virtual ToolSession* active_tool_session() = 0;
  virtual Pane* active_map() = 0;
  virtual void activate_inspector_tab(int index) = 0;
  virtual ui::views::TabStrip* inspector_tabs() = 0;
  virtual bool note_product_map2d_surface_after_show() = 0;
  virtual void ensure_china_maplibre_carto() = 0;
  virtual void frame_china_map2d(int view_w, int view_h) = 0;
  virtual void apply_china_scene3d_product_defaults() = 0;
  virtual void apply_china_scene3d_orbit() = 0;

  virtual bool has_plugins() const = 0;
  virtual void ensure_plugins() = 0;
  virtual std::string eval_python(const std::string& code) = 0;
  virtual PluginHost* plugin_host() = 0;
  virtual bool ensure_command(const char* command_id) = 0;
  virtual bool run_processing(const std::string& id, const std::string& args) = 0;
  virtual bool apply_plugin_frame(int index) = 0;
  virtual int export_plugin_frames(const std::string& dir_leaf) = 0;

  // Publishes this shell as plugin::HarnessShell for |fn|.
  virtual int with_harness_shell(int (*fn)(plugin::HarnessShell&)) = 0;
  virtual int dispatch_plugin_command(const char* command_id,
                                      std::string_view payload) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_BIND_SHELL_BRIDGE_H_
