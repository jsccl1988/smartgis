// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "app/views/camera/map_host_extent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/dem/dem_commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/runtime/host/registry.h"
#include "app/views/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/views/dialogs/gis/add_basemap_dialog.h"
#include "ui/views/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/dialogs/gis/att_struct_dialog.h"
#include "ui/views/gis/inspect/attribute_table.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/gis/create_datasource_dialog.h"
#include "ui/views/dialogs/gis/create_layer_dialog.h"
#include "ui/views/dialogs/gis/create_map_dialog.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/input_text_dialog.h"
#include "ui/views/gis/catalog/layer_tree.h"
#include "ui/views/gis/panel/processing_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

// Map, Data, and 3D page attach, invalidate, shared extent, and gesture wiring.

void BrowserView::attach_viewports() {
  // Ensure every map tab page has a real client rect before OpenView/Resize
  // (inactive tabs used to keep 0x0 bounds).
  widget_.layout_contents();

  struct Bind {
    ui::views::MapViewport* pane;
    content::ViewHost* host;
    const char* tool;
  };
  const Bind binds[] = {
      {map_edit_, browser_->edit_host(), "view.pan"},
      {map_data_, browser_->data_host(), "view.pan"},
      {map_scene_, browser_->scene_host(), "view3d.trackball"},
  };
  for (const Bind& b : binds) {
    if (!b.pane) {
      continue;
    }
    b.pane->set_view_host(b.host);
    if (browser_->map_session()) {
      b.pane->set_map_contents(browser_->map_session());
    }
    b.pane->attach();
    if (b.host) {
      b.host->activate(b.tool);
    }
  }
  widget_.layout_contents();
  for_each_map_viewport([](ui::views::MapViewport* pane) {
    if (!pane->native_view()) {
      return;
    }
    pane->sync_native_bounds();
    RECT rc = {};
    GetClientRect(pane->native_view(), &rc);
    if (rc.right > 0 && rc.bottom > 0) {
      SendMessageW(pane->native_view(), WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(rc.right, rc.bottom));
    }
  });
  // Prefetch leftover GL stereo only when FlyCube is not the Scene3d SoT.
  // Attaching stereo under FlyCube races the DX12 HWND and has corrupted heaps.
  if (map_scene_ && map_scene_->native_view() && !prefer_scene3d_flycube()) {
    (void)browser_->scene3d_stereo()->try_attach(map_scene_->native_view());
  }
}

void BrowserView::wire_map_scene() {
  auto paint2d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (browser_->blit()->in_preview() && browser_->blit()->present(hdc, w, h)) {
      return;
    }
    // Shared browser_->document(): present_gpu sets last_gpu_present_ok on success.
    // SMT_FORCE_GDI_MAP_OVERLAY=1 skips 2D gpu_present_ in MapViewport.
    const bool force_gdi = []() {
      if (const char* env = std::getenv("SMT_FORCE_GDI_MAP_OVERLAY")) {
        return env[0] == '1' && env[1] == '\0';
      }
      return false;
    }();
    if (!force_gdi && browser_->map2d()->last_gpu_present_ok()) {
      browser_->map2d()->paint_annotation_overlay(hdc, w, h);
    } else {
      browser_->map2d()->paint(hdc, w, h, true);
      browser_->blit()->capture(hdc, w, h);
    }
    content::ViewHost* host = active_view_host();
    if (browser_->flash_lit() && host && host->workspace() &&
        host->workspace()->flashing()) {
      browser_->map2d()->paint_flash_overlay(hdc, w, h);
    }
  };
  auto paint3d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return;
    }
    const auto mode = map_scene_ ? map_scene_->attach_mode()
                                 : ui::views::MapViewport::AttachMode::kNone;
    // Priority: FlyCube → leftover GL stereo → ContentMapView DIB → GDI DEM.
    const bool flycube = mode == ui::views::MapViewport::AttachMode::kFlyCube;
    const bool content_map =
        mode == ui::views::MapViewport::AttachMode::kContentMapView;
    const bool gpu_ok =
        flycube && map_scene_ && map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    HWND hwnd = map_scene_ ? map_scene_->native_view() : nullptr;
    if (browser_->scene3d_stereo()->try_present_sot(
            hwnd, hdc, w, h, browser_->orbit_frame()->yaw(),
            browser_->orbit_frame()->pitch(),
            browser_->orbit_frame()->distance())) {
      browser_->scene3d()->set_render_engine_name("Stereo/GL");
      browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    if (content_map) {
      // MapViewport already blitted ContentMapView when possible; HUD only.
      browser_->scene3d()->set_render_engine_name("ContentMapView");
      browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    browser_->scene3d()->paint(hdc, w, h, /*fill_background=*/true);
  };
  for (ui::views::MapViewport* pane : {map_edit_, map_data_}) {
    if (!pane) {
      continue;
    }
    pane->set_overlay_paint(paint2d);
    // Per-pane shell: shared Map2dPresenter content, pane-local DrawRequest.shell.
    pane->set_gpu_present(
        [this, pane](void* device, uint32_t w, uint32_t h) -> bool {
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr = nullptr;
          if (pane->snapshot_shell_overlay(&shell_copy, &shell, &gen)) {
            shell_ptr = &shell;
          }
          return browser_->map2d()->present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  if (map_scene_) {
    map_scene_->set_overlay_paint(paint3d);
    map_scene_->set_gpu_present(
        [this](void* device, uint32_t w, uint32_t h) -> bool {
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr = nullptr;
          if (map_scene_->snapshot_shell_overlay(&shell_copy, &shell, &gen)) {
            shell_ptr = &shell;
          }
          return browser_->scene3d()->present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  // Seed shell overlay Commit for FlyCube / PresentMailbox (generation skip).
  commit_widget_shell_to_maps();
  wire_tool_seams();
  sync_inspectors_from_scene();
}

void BrowserView::commit_widget_shell_to_maps() {
  // Full-client seed (tab switch / overlay invalidate).
  commit_widget_shell_to_maps(ui::views::Rect{});
}

void BrowserView::commit_widget_shell_to_maps(const ui::views::Rect& dirty) {
  const ui::gfx::ShellRaster shell = widget_.shell_raster();
  const std::uint64_t gen = widget_.shell_generation();
  if (!shell.bgra || shell.width_px == 0 || shell.height_px == 0) {
    return;
  }
  const uint32_t shell_stride =
      shell.stride_bytes != 0 ? shell.stride_bytes : shell.width_px * 4u;
  HWND widget_hwnd = widget_.hwnd();
  // Empty dirty ⇒ publish to every pane. Non-empty ⇒ skip panes the paint did
  // not touch so menu/button hover does not memcpy+wake map HWNDs.
  const bool filter = !dirty.is_empty();
  for_each_map_viewport([&](ui::views::MapViewport* pane) {
    HWND pane_hwnd = pane->native_view();
    RECT pane_in_widget = {};
    if (pane_hwnd && widget_hwnd) {
      GetClientRect(pane_hwnd, &pane_in_widget);
      MapWindowPoints(pane_hwnd, widget_hwnd,
                      reinterpret_cast<POINT*>(&pane_in_widget), 2);
    } else {
      const ui::views::Rect& b = pane->bounds();
      pane_in_widget = {b.x, b.y, b.x + b.width, b.y + b.height};
    }
    const ui::views::Rect pane_rect{
        pane_in_widget.left, pane_in_widget.top,
        pane_in_widget.right - pane_in_widget.left,
        pane_in_widget.bottom - pane_in_widget.top};
    if (filter && !dirty.intersects(pane_rect)) {
      return;
    }
    // Crop widget shell to the map HWND so DrawRequest.shell matches the
    // surface (attach_shell_raster / overlay require equal size).
    int x0 = pane_rect.x;
    int y0 = pane_rect.y;
    int crop_w = pane_rect.width;
    int crop_h = pane_rect.height;
    if (x0 < 0) {
      crop_w += x0;
      x0 = 0;
    }
    if (y0 < 0) {
      crop_h += y0;
      y0 = 0;
    }
    if (x0 + crop_w > static_cast<int>(shell.width_px)) {
      crop_w = static_cast<int>(shell.width_px) - x0;
    }
    if (y0 + crop_h > static_cast<int>(shell.height_px)) {
      crop_h = static_cast<int>(shell.height_px) - y0;
    }
    if (crop_w <= 0 || crop_h <= 0) {
      return;
    }
    const uint8_t* crop =
        shell.bgra + static_cast<size_t>(y0) * shell_stride +
        static_cast<size_t>(x0) * 4u;
    pane->commit_shell_overlay(crop, static_cast<uint32_t>(crop_w),
                               static_cast<uint32_t>(crop_h), shell_stride,
                               gen);
  });
}

void BrowserView::sync_flash_timer() {
  HWND h = hwnd();
  if (!h) {
    return;
  }
  constexpr UINT_PTR kFlash = 0x464C5348u;
  SetPropW(h, L"SmtFlashBrowser", reinterpret_cast<HANDLE>(this));
  content::ViewHost* host = active_view_host();
  const bool on = host && host->workspace() && host->workspace()->flashing();
  KillTimer(h, kFlash);
  if (!on) {
    browser_->set_flash_lit(true);
    return;
  }
  SetTimer(h, kFlash, 400, [](HWND hwnd, UINT, UINT_PTR, DWORD) {
    auto* self = reinterpret_cast<BrowserView*>(
        GetPropW(hwnd, L"SmtFlashBrowser"));
    if (!self) {
      return;
    }
    self->browser_->set_flash_lit(!self->browser_->flash_lit());
    self->invalidate_map_overlays();
  });
}

void BrowserView::wire_tool_seams() {
  auto resolve = [this](const tool::Draft& draft) -> content::FeatureId {
    content::ViewHost* host = active_view_host();
    tool::Interaction* cur =
        host && host->workspace() ? host->workspace()->stack().current()
                                  : nullptr;
    const char* tool_id = cur ? cur->id() : "";
    if (!tool_id || draft.points.empty()) {
      return {};
    }
    const int px = draft.points.front().x_px;
    const int py = draft.points.front().y_px;
    double map_x = 0;
    double map_y = 0;
    browser_->view_frame()->view_to_map(px, py, &map_x, &map_y);
    const double scale =
        browser_->view_frame()->scale() > 1e-9 ? browser_->view_frame()->scale() : 1.0;
    const double tol_map = 12.0 / scale;
    if (std::strncmp(tool_id, "select.", 7) == 0) {
      const MapScene::Feature* hit =
          browser_->document()->hit_test(map_x, map_y, tol_map);
      return hit ? hit->id : content::FeatureId{};
    }
    if (std::strcmp(tool_id, "edit.vertex") == 0) {
      return browser_->document()->move_selected_vertex(map_x, map_y, tol_map);
    }
    return {};
  };
  auto nav = [this](std::string_view command_id) -> content::Extent2 {
    int w = 800;
    int h = 600;
    active_view_size(&w, &h);
    if (command_id == "view.full" || command_id == "view3d.full") {
      browser_->view_frame()->fit_extent(*browser_->document(), w, h);
      browser_->orbit_frame()->apply_world_extent(
          browser_->document()->world_extent());
      browser_->push_shared_extent();
      invalidate_map_overlays();
    } else if (command_id == "view.refresh") {
      browser_->view_frame()->apply_world_extent(
          browser_->view_frame()->view_world_extent(w, h), w, h);
      browser_->push_shared_extent();
      invalidate_map_overlays();
    }
    return browser_->view_frame()->view_world_extent(w, h);
  };
  auto on_draft = [this](const tool::Draft& draft) {
    browser_->handle_draft(draft);
  };
  for (content::ViewHost* host : {browser_->edit_host(), browser_->data_host(),
                                   browser_->scene_host()}) {
    if (!host || !host->workspace()) {
      continue;
    }
    tool::Workspace* ws = host->workspace();
    ws->set_draft_observer(on_draft);
    ws->set_feature_hit(resolve);
    ws->set_nav_command(nav);
    ws->set_shell_owns_append(true);
  }
}

void BrowserView::for_each_map_viewport(
    const std::function<void(ui::views::MapViewport*)>& fn) const {
  if (!fn) {
    return;
  }
  for (ui::views::MapViewport* pane : {map_edit_, map_data_, map_scene_}) {
    if (pane) {
      fn(pane);
    }
  }
}

void BrowserView::invalidate_map_overlays() {
  // Keep shell generation in sync when map panes redraw without a shell paint.
  commit_widget_shell_to_maps();
  for_each_map_viewport([](ui::views::MapViewport* pane) {
    pane->invalidate_native();
  });
}

void BrowserView::attach_hwnd_gestures() {
  auto on_pinch = [this](int x, int y, double scale) {
    browser_->handle_pinch(x, y, scale);
  };
  auto on_pan = [this](int dx, int dy) {
    browser_->handle_gesture_pan(dx, dy);
  };
  if (map_edit_ && map_edit_->native_view()) {
    browser_->edit_gestures()->attach(map_edit_->native_view(), on_pinch, on_pan);
    configure_gestures(browser_->edit_gestures());
  }
  if (map_data_ && map_data_->native_view()) {
    browser_->data_gestures()->attach(map_data_->native_view(), on_pinch, on_pan);
    configure_gestures(browser_->data_gestures());
  }
  if (map_scene_ && map_scene_->native_view()) {
    browser_->scene_gestures()->attach(map_scene_->native_view(), on_pinch,
                                       on_pan);
    configure_gestures(browser_->scene_gestures());
  }
}

void BrowserView::configure_gestures(MapHwndGestures* gestures) {
  if (!gestures) {
    return;
  }
  gestures->set_right_click([this](HWND map_hwnd, int x, int y) {
    on_map_right_click(map_hwnd, x, y);
  });
  gestures->set_extent_watch(
      [this](bool begin) { browser_->on_extent_watch(begin); });
  gestures->set_viewport_resized([this]() { browser_->refresh_scale(); });
}

void BrowserView::active_view_size(int* w, int* h) const {
  int width = 800;
  int height = 600;
  if (ui::views::MapViewport* pane = active_map()) {
    if (HWND hwnd = pane->native_view()) {
      RECT rc = {};
      GetClientRect(hwnd, &rc);
      if (rc.right > 32) {
        width = rc.right;
      }
      if (rc.bottom > 32) {
        height = rc.bottom;
      }
    }
  }
  if (w) {
    *w = width;
  }
  if (h) {
    *h = height;
  }
}

void BrowserView::switch_map_tab(int i) {
  if (map_tabs_) {
    map_tabs_->set_active(i);
    map_tabs_->layout();
  }
  // TabStrip show/hides native map HWNDs via View::set_visible; also force
  // Win32 visibility so self-test / rapid tab switches cannot leave the active
  // pane hidden when sync_native_bounds skips a no-op SetWindowPos.
  auto sync_hwnd = [](ui::views::MapViewport* pane, bool show) {
    if (!pane) {
      return;
    }
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
      }
    }
  };
  sync_hwnd(map_edit_, i == 0);
  sync_hwnd(map_data_, i == 1);
  sync_hwnd(map_scene_, i == 2);
  // TabStrip show/hides native map HWNDs; never destroy/recreate on switch.
  if (ui::views::MapViewport* pane = active_map()) {
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        // Force a size notify so content/FlyCube surfaces follow the tab body.
        if (rc.right > 0 && rc.bottom > 0) {
          SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED,
                       MAKELPARAM(rc.right, rc.bottom));
        }
      }
    }
  }
  if (content::ViewHost* host = active_view_host()) {
    if (i == 2) {
      // Basic pan/orbit for the 3D tab when a ViewHost is wired.
      host->activate("view3d.trackball");
      // WinUI show_kind parity: re-bind Scene3d view id + China extent so DEM
      // seed / present_gpu / GDI paint share the same world frame.
      if (map_scene_) {
        browser_->scene3d()->bind_contents(browser_->map_session(), map_scene_->view_id());
        if (HWND hwnd = map_scene_->native_view()) {
          if (!prefer_scene3d_flycube()) {
            (void)browser_->scene3d_stereo()->try_attach(hwnd);
          } else {
            browser_->scene3d_stereo()->release();
          }
        }
      }
      // Recover from edge-on / over-zoomed orbit (thin green DEM strip).
      browser_->orbit_frame()->reset();
      browser_->push_shared_extent();
      if (map_scene_) {
        map_scene_->invalidate_native();
      }
    } else {
      host->activate("view.pan");
    }
  }
  sync_status();
}

ui::views::MapViewport* BrowserView::active_map() const {
  const int i = map_tabs_ ? map_tabs_->active() : 0;
  if (i == 1) {
    return map_data_;
  }
  if (i == 2) {
    return map_scene_;
  }
  return map_edit_;
}

content::ViewHost* BrowserView::active_view_host() const {
  const int i = map_tabs_ ? map_tabs_->active() : 0;
  if (i == 1) {
    return browser_->data_host();
  }
  if (i == 2) {
    return browser_->scene_host();
  }
  return browser_->edit_host();
}

}  // namespace app
