// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shell access bag the IL host bind path fills into CapabilityHost.
// Concrete Ops + pointers — no virtual Shell ABI. app/views owns the
// only filler; this header does not include app/views.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_SHELL_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_SHELL_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/capability/op.h"
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

// Browser-shaped Op bag used while binding CapabilityHost and while a suite
// runs. Shell registration fills every slot; empty Ops are false / no-ops.
struct Shell {
  detail::Op<GisScene*()> document;
  detail::Op<Map2dPresenter*()> map2d;
  detail::Op<Scene3dPresenter*()> scene3d;
  detail::Op<ViewFrame*()> view_frame;
  detail::Op<OrbitFrame*()> orbit_frame;
  // Pointer (not reference): detail::Op cannot value-init a reference on empty.
  detail::Op<BrowserSession*()> session;
  detail::Op<HWND()> hwnd;
  detail::Op<Pane*()> draw_host;
  detail::Op<Pane*()> data_draw_host;
  detail::Op<Pane*()> scene_draw_host;
  detail::Op<ToolSession*()> edit_tool_session;
  detail::Op<ToolSession*()> data_tool_session;
  detail::Op<ToolSession*()> scene_tool_session;
  detail::Op<bool(std::string_view)> run_tool_command;
  detail::Op<void(int)> select_view_tab;
  detail::Op<void(std::string_view, int, bool, int, int)> on_view_command;
  detail::Op<void()> fit_map_extent;
  detail::Op<void(std::uint32_t)> pump;
  detail::Op<ui::views::View*()> contents_view;
  detail::Op<ui::views::CatalogView*()> catalog_view;
  detail::Op<ui::views::StatusBar*()> status_bar;
  detail::Op<GisContents*()> gis_contents;
  detail::Op<BlitFrameCache*()> blit;

  detail::Op<bool()> has_ui;
  detail::Op<void()> invalidate_native_map;
  detail::Op<void()> invalidate_map_overlays;
  detail::Op<void()> schedule_overlay_full_redraw;
  detail::Op<void(const std::function<void(Pane*)>&)> for_each_draw_host;
  detail::Op<ToolSession*()> active_tool_session;
  detail::Op<Pane*()> active_map;
  detail::Op<void(int)> activate_inspector_tab;
  detail::Op<ui::views::TabStrip*()> inspector_tabs;
  detail::Op<bool()> note_product_map2d_surface_after_show;
  detail::Op<void()> ensure_china_maplibre_carto;
  detail::Op<void(int, int)> frame_china_map2d;
  detail::Op<void()> apply_china_scene3d_product_defaults;
  detail::Op<void()> apply_china_scene3d_orbit;

  detail::Op<bool()> has_plugins;
  detail::Op<void()> ensure_plugins;
  detail::Op<std::string(const std::string&)> eval_python;
  detail::Op<PluginHost*()> plugin_host;
  detail::Op<bool(const char*)> ensure_command;
  detail::Op<bool(const std::string&, const std::string&)> run_processing;
  detail::Op<bool(int)> apply_plugin_frame;
  detail::Op<int(const std::string&)> export_plugin_frames;

  // Publishes this shell as plugin::HarnessShell for |fn|.
  detail::Op<int(int (*)(plugin::HarnessShell&))> with_harness_shell;
  detail::Op<int(const char*, std::string_view)> dispatch_plugin_command;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_SHELL_H_
