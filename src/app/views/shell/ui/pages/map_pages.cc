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

#include "base/core/log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "content/browser/camera/map_host_extent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/dem/commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/runtime/host/registry.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"
#include "gis/vista/world/terrain/land_mask.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/dialogs/add_basemap_dialog.h"
#include "ui/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/dialogs/att_struct_dialog.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/dialogs/create_datasource_dialog.h"
#include "ui/gis/dialogs/create_layer_dialog.h"
#include "ui/gis/dialogs/create_map_dialog.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

// Map, Data, and 3D page attach, invalidate, shared extent, and gesture wiring.

void BrowserView::attach_viewports() {
  // Ensure every map tab page has a real client rect before OpenView/Resize
  // (inactive tabs used to keep 0x0 bounds).
  widget_.layout_contents();
  LOGGING(LOG_INFO, "rhi.attach_viewports: layout done; Map Edit attaches now");

  struct Bind {
    ui::views::MapViewport* pane;
    content::ViewHost* host;
    const char* tool;
    bool attach_now;
  };
  // Only DX12-init the visible Map Edit pane at startup. Data + 3D realize
  // HWND only — three FlyCube devices each busy-waited up to ~5s and made
  // SmartGisViews feel stuck on launch (debug D3D12 layers amplify this).
  // Realize + second layout BEFORE attach so FlyCube Init samples the tab-body
  // client size (not a stale multi-k px rect that leaves a navy-clear present).
  const Bind binds[] = {
      {map_edit_, browser_->edit_host(), "view.pan", true},
      {map_data_, browser_->data_host(), "view.pan", false},
      {map_scene_, browser_->scene_host(), "view3d.trackball", false},
  };
  for (const Bind& b : binds) {
    if (!b.pane) {
      continue;
    }
    b.pane->set_view_host(b.host);
    if (browser_->map_session()) {
      b.pane->set_map_contents(browser_->map_session());
    }
    if (!b.pane->native_view()) {
      b.pane->realize_native();
    }
    if (!b.attach_now) {
      if (HWND hwnd = b.pane->native_view()) {
        ShowWindow(hwnd, SW_HIDE);
      }
    }
  }
  widget_.layout_contents();
  for (const Bind& b : binds) {
    if (!b.pane || !b.attach_now) {
      continue;
    }
    b.pane->sync_native_bounds();
    b.pane->attach();
    if (b.host) {
      b.host->activate(b.tool);
    }
  }
  for_each_map_viewport([](ui::views::MapViewport* pane) {
    if (!pane->native_view()) {
      return;
    }
    pane->sync_native_bounds();
    // Size-notify only panes that already own a present device; deferred
    // Data/3D attach on first tab focus.
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kNone) {
      return;
    }
    RECT rc = {};
    GetClientRect(pane->native_view(), &rc);
    if (rc.right > 0 && rc.bottom > 0) {
      SendMessageW(pane->native_view(), WM_SIZE, SIZE_RESTORED,
                   MAKELPARAM(rc.right, rc.bottom));
    }
  });
  // Prefetch leftover GL stereo only when Stereo/GL is the selected engine.
  // Attaching stereo under FlyCube races the DX12 HWND and has corrupted heaps.
  // Scene3d FlyCube itself is deferred until the 3D tab is focused.
  if (map_scene_ && map_scene_->native_view() &&
      map_scene_->attach_mode() != ui::views::MapViewport::AttachMode::kNone &&
      prefer_scene3d_stereo_gl()) {
    (void)browser_->scene3d_stereo()->try_attach(map_scene_->native_view());
  }
}

void BrowserView::wire_map_scene() {
  auto paint2d_for = [this](ui::views::MapViewport* pane) {
    return [this, pane](HDC hdc, const RECT& rc) {
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      if (browser_->blit()->in_preview() &&
          browser_->blit()->present(hdc, w, h)) {
        return;
      }
      // GDI overlay paints into |hdc| (backbuffer DIB). FlyCube present_gpu
      // writes the DXGI swapchain — last_gpu_present_ok must NOT skip full
      // GDI here or the DIB stays teal/empty (annotations only). Only skip
      // full GDI when FlyCube 2D actually presented this viewport; ContentMapView
      // SharedSurface often lands as ocean-only without vector fills.
      const bool force_gdi = []() {
        if (const char* env = std::getenv("SMT_FORCE_GDI_MAP_OVERLAY")) {
          return env[0] == '1' && env[1] == '\0';
        }
        return false;
      }();
      const bool flycube_sot =
          pane &&
          pane->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube &&
          pane->last_gpu_present_ok();
      if (!force_gdi && flycube_sot) {
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
  };
  auto paint3d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return;
    }
    const auto mode = map_scene_ ? map_scene_->attach_mode()
                                 : ui::views::MapViewport::AttachMode::kNone;
    // Priority: FlyCube → (opt-in) leftover GL stereo → ContentMapView → GDI DEM.
    const bool flycube = mode == ui::views::MapViewport::AttachMode::kFlyCube;
    const bool content_map =
        mode == ui::views::MapViewport::AttachMode::kContentMapView;
    const bool gpu_ok =
        flycube && map_scene_ && map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    // When FlyCube is the product SoT (default), never re-attach leftover GL
    // on the same HWND — try_present_sot would race the DX12 swapchain and
    // permanently stamp the HUD badge as Stereo/GL even after RHI recovers.
    // Stereo/GL is only allowed when the user selected that engine.
    const bool allow_stereo_fallback =
        prefer_scene3d_stereo_gl() && !flycube;
    HWND hwnd = map_scene_ ? map_scene_->native_view() : nullptr;
    if (allow_stereo_fallback &&
        browser_->scene3d_stereo()->try_present_sot(
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
    // FlyCube attached but present not yet ok (or failed): software DEM SoT
    // into the paint DC. Keep the RHI label so the badge is not "Stereo/GL".
    if (flycube) {
      browser_->scene3d()->set_render_engine_name("FlyCube/DX12");
    }
    browser_->scene3d()->paint(hdc, w, h, /*fill_background=*/true);
  };
  for (ui::views::MapViewport* pane : {map_edit_, map_data_}) {
    if (!pane) {
      continue;
    }
    pane->set_overlay_paint(paint2d_for(pane));
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
  // Empty dirty �?publish to every pane. Non-empty �?skip panes the paint did
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
    // Opaque Theme clear under native map holes must not src-over FlyCube.
    const ui::views::Theme& theme = ui::views::Theme::current();
    pane->commit_shell_overlay(crop, static_cast<uint32_t>(crop_w),
                               static_cast<uint32_t>(crop_h), shell_stride, gen,
                               theme.shell_bg, theme.map_placeholder);
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
    ws->set_map_project([this](int x_px, int y_px, double* map_x,
                               double* map_y) {
      browser_->view_frame()->view_to_map(x_px, y_px, map_x, map_y);
    });
    // β: DraftPipeline owns FeatureGeom → EditSession; MapScene still mirrors
    // via handle_draft append_from_draft (no second EditSession commit).
    ws->set_shell_owns_append(false);
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
  // Only wire gestures for panes that already own a present device. Data/3D
  // are HWND-only until first tab focus (see attach_viewports / switch_map_tab).
  auto try_attach = [&](ui::views::MapViewport* pane, MapHwndGestures* g) {
    if (!pane || !g || !pane->native_view()) {
      return;
    }
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kNone) {
      return;
    }
    g->attach(pane->native_view(), on_pinch, on_pan);
    configure_gestures(g);
  };
  try_attach(map_edit_, browser_->edit_gestures());
  try_attach(map_data_, browser_->data_gestures());
  try_attach(map_scene_, browser_->scene_gestures());
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
  // Tab body bounds must be current before FlyCube Init / ShowWindow — deferred
  // Data/3D panes were realize_native'd hidden; a stale 1x1 client makes DX12
  // attach "succeed" then present a blank swapchain (双击启动切 3D 无画面).
  widget_.layout_contents();
  if (map_tabs_) {
    map_tabs_->layout();
  }

  // TabStrip show/hides native map HWNDs via View::set_visible; also force
  // Win32 visibility so self-test / rapid tab switches cannot leave the active
  // pane hidden when sync_native_bounds skips a no-op SetWindowPos.
  auto sync_hwnd = [](ui::views::MapViewport* pane, bool show) {
    if (!pane) {
      return;
    }
    pane->sync_native_bounds();
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
        if (show) {
          // Sibling Map Edit FlyCube HWND can paint above a newly shown 3D
          // child when z-order is left unchanged after SW_HIDE/SW_SHOW.
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
      }
    }
    // Owned DXGI present popups are top-level — hiding the embed alone leaves
    // Map-Edit's present covering Scene3d (navy clear / wrong SoT).
    pane->set_flycube_present_visible(show);
  };
  sync_hwnd(map_edit_, i == 0);
  sync_hwnd(map_data_, i == 1);
  sync_hwnd(map_scene_, i == 2);

  // Lazy FlyCube: Data/3D were HWND-only at startup.
  if (ui::views::MapViewport* pane = active_map()) {
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kNone) {
      LOGGING(LOG_INFO, "rhi.switch_map_tab lazy attach tab=%d", i);
      // Size + show before Init so GetClientRect is the tab body, not 1x1.
      pane->sync_native_bounds();
      if (HWND hwnd = pane->native_view()) {
        if (IsWindow(hwnd)) {
          ShowWindow(hwnd, SW_SHOW);
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
      }
      pane->attach();
      pane->sync_native_bounds();
      // Gestures were skipped while AttachMode::kNone.
      auto on_pinch = [this](int x, int y, double scale) {
        browser_->handle_pinch(x, y, scale);
      };
      auto on_pan = [this](int dx, int dy) {
        browser_->handle_gesture_pan(dx, dy);
      };
      MapHwndGestures* g = nullptr;
      if (pane == map_data_) {
        g = browser_->data_gestures();
      } else if (pane == map_scene_) {
        g = browser_->scene_gestures();
      } else {
        g = browser_->edit_gestures();
      }
      if (g && pane->native_view()) {
        g->attach(pane->native_view(), on_pinch, on_pan);
        configure_gestures(g);
      }
    }
  }

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

  // 3D tab: seed atmosphere / DEM frame even when ViewHost wiring is late —
  // previously the whole block lived under active_view_host() and a null host
  // skipped enable_demo + orbit reset (blank DX12 present).
  if (i == 2 && map_scene_) {
    browser_->scene3d()->bind_contents(browser_->map_session(),
                                       map_scene_->view_id());
    if (HWND hwnd = map_scene_->native_view()) {
      if (prefer_scene3d_stereo_gl()) {
        (void)browser_->scene3d_stereo()->try_attach(hwnd);
      } else {
        browser_->scene3d_stereo()->release();
      }
    }
    // Drop any mesh built under a 2D Map-Edit crop before China framing.
    browser_->scene3d()->abandon_mesh();
    // Seed atmosphere before tool activate — trackball activation can emit a
    // draft that nudges yaw away from the China framing below.
    browser_->scene3d()->atmosphere_session().seed_procedural(
        /*with_land_rings=*/true);
    std::vector<gis::LonLatRing> land_rings;
    if (browser_->document()) {
      browser_->document()->export_land_rings(&land_rings);
    }
    // Default: ocean + sky + soft cloud deck (showcase-full look with living
    // water). Opt out with SMT_SCENE3D_LAND_ONLY=1 or SMT_SCENE3D_ATMO=0.
    // Fog stays off by default (washes DEM hypsometric greens); toggle via
    // Atmosphere panel. Cloud cover is capped in prepare_clouds so sky/DEM
    // still read.
    bool ocean_ok = true;
    bool sky_ok = true;
    bool cloud_ok = true;
    if (const char* atmo = std::getenv("SMT_SCENE3D_ATMO");
        atmo && atmo[0] == '0' && atmo[1] == '\0') {
      ocean_ok = false;
      sky_ok = false;
      cloud_ok = false;
    }
    if (const char* land_only = std::getenv("SMT_SCENE3D_LAND_ONLY");
        land_only && land_only[0] == '1' && land_only[1] == '\0') {
      ocean_ok = false;
      sky_ok = false;
      cloud_ok = false;
    }
    browser_->scene3d()->atmosphere_session().set_ocean_enabled(ocean_ok);
    browser_->scene3d()->atmosphere_session().set_cloud_enabled(cloud_ok);
    browser_->scene3d()->atmosphere_session().set_sky_enabled(sky_ok);
    browser_->scene3d()->atmosphere_session().set_fog_enabled(false);
    if (atmosphere_panel_) {
      atmosphere_panel_->set_ocean_checked(ocean_ok);
      atmosphere_panel_->set_cloud_checked(cloud_ok);
      atmosphere_panel_->set_sky_checked(sky_ok);
      atmosphere_panel_->set_fog_checked(false);
    }
    LOGGING(LOG_INFO,
            "rhi.switch_map_tab scene3d rings=%zu ocean=%d cloud=%d sky=%d",
            land_rings.size(), ocean_ok ? 1 : 0, cloud_ok ? 1 : 0,
            sky_ok ? 1 : 0);
  }

  if (content::ViewHost* host = active_view_host()) {
    if (i == 2) {
      host->activate("view3d.trackball");
    } else {
      host->activate("view.pan");
    }
  }

  // Orbit framing AFTER tool activate — activate("view3d.trackball") historically
  // left yaw≈-0.42 (blank/navy interactive present) while showcase keeps ~2.59.
  if (i == 2 && map_scene_ && browser_) {
    browser_->orbit_frame()->reset();
    browser_->orbit_frame()->apply_world_extent(content::kChinaLonLatExtent);
    browser_->orbit_frame()->set_distance(2.55f);
    browser_->push_shared_extent();
    LOGGING(LOG_INFO,
            "rhi.switch_map_tab scene3d orbit yaw=%.2f pitch=%.2f dist=%.2f",
            browser_->orbit_frame()->yaw(), browser_->orbit_frame()->pitch(),
            browser_->orbit_frame()->distance());
    map_scene_->invalidate_native();
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
