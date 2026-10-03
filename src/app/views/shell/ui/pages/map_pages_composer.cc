// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/pages/map_pages_composer.h"
#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"

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
#include "content/browser/debug/debug_agent.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/runtime/host/registry/registry.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "gis/vista/domain/atmosphere/systems/environment.h"
#include "gis/vista/world/terrain/process/land_mask.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/add_basemap_dialog.h"
#include "ui/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/inspect/attribute_schema_dialog.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/create_datasource_dialog.h"
#include "ui/gis/catalog/create_layer_dialog.h"
#include "ui/gis/catalog/create_map_dialog.h"
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
namespace {

// SEH helpers must live in a TU function with no C++ object unwinding
// (MSVC C2712). Used when MapSession / ViewHost ABI drifts across partial
// multi-agent out/Debug rebuilds.

bool ptr_addr_poison(uintptr_t addr) {
  if (addr < 0x10000u) {
    return true;
  }
  const auto lo24 = addr & 0xffffff00ull;
  return lo24 == 0xcdcdcd00ull || lo24 == 0xdddddd00ull ||
         lo24 == 0xcccccc00ull || lo24 == 0xfeeefeeeull ||
         lo24 == 0xababab00ull;
}

bool ptr_mem_readable(const void* p, size_t nbytes) {
  if (!p || nbytes == 0) {
    return false;
  }
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
    return false;
  }
  if (mbi.State != MEM_COMMIT) {
    return false;
  }
  const DWORD prot = mbi.Protect & 0xffu;
  if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE || prot == PAGE_GUARD) {
    return false;
  }
  const auto* base = static_cast<const uint8_t*>(mbi.BaseAddress);
  const auto* end = static_cast<const uint8_t*>(p) + nbytes;
  return end <= base + mbi.RegionSize;
}

tool::Workspace* seh_view_host_workspace(content::ViewHost* host) {
  __try {
    return host->workspace();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }
}

bool seh_view_host_flashing(content::ViewHost* host) {
  if (!host || ptr_addr_poison(reinterpret_cast<uintptr_t>(host))) {
    return false;
  }
  __try {
    return host->flashing();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

struct WorkspaceBindFns {
  void (*set_draft)(tool::Workspace*, void*);
  void (*set_hit)(tool::Workspace*, void*);
  void (*set_nav)(tool::Workspace*, void*);
  void (*set_project)(tool::Workspace*, void*);
  void* draft_ctx;
  void* hit_ctx;
  void* nav_ctx;
  void* project_ctx;
};

bool seh_bind_workspace(tool::Workspace* ws, WorkspaceBindFns* fns) {
  __try {
    fns->set_draft(ws, fns->draft_ctx);
    fns->set_hit(ws, fns->hit_ctx);
    fns->set_nav(ws, fns->nav_ctx);
    fns->set_project(ws, fns->project_ctx);
    ws->set_shell_owns_append(false);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

void trampoline_set_draft(tool::Workspace* ws, void* ctx) {
  ws->set_draft_observer(*static_cast<tool::DraftCallback*>(ctx));
}

void trampoline_set_hit(tool::Workspace* ws, void* ctx) {
  ws->set_feature_hit(*static_cast<tool::Workspace::FeatureHit*>(ctx));
}

void trampoline_set_nav(tool::Workspace* ws, void* ctx) {
  ws->set_nav_command(*static_cast<tool::Workspace::NavCommand*>(ctx));
}

void trampoline_set_project(tool::Workspace* ws, void* ctx) {
  ws->set_map_project(*static_cast<tool::Workspace::MapProject*>(ctx));
}

}  // namespace

// Map, Data, and 3D page attach, invalidate, shared extent, and gesture wiring.

// Map/Data/3D tab chrome: viewports, gestures, overlays, tool seams.
MapPagesComposer::MapPagesComposer(BrowserView* host) : host_(host) {}

void MapPagesComposer::attach_viewports() {
  // Ensure every map tab page has a real client rect before OpenView/Resize
  // (inactive tabs used to keep 0x0 bounds).
  host_->widget_.layout_contents();
  LOGGING(LOG_INFO, "rhi.attach_viewports: layout done; Map Edit attaches now");

  struct Bind {
    ui::views::MapViewport* pane;
    content::ViewHost* host;
    const char* tool;
    bool attach_now;
  };
  // Only DX12-init the visible Map Edit pane at startup. Data + 3D realize
  // HWND only ï¿?three FlyCube devices each busy-waited up to ~5s and made
  // SmartGisViews feel stuck on launch (debug D3D12 layers amplify this).
  // Realize + second layout BEFORE attach so FlyCube Init samples the tab-body
  // client size (not a stale multi-k px rect that leaves a navy-clear present).
  const Bind binds[] = {
      {host_->map_edit_, host_->browser_->edit_host(), "view.pan", true},
      {host_->map_data_, host_->browser_->data_host(), "view.pan", false},
      {host_->map_scene_, host_->browser_->scene_host(), "view3d.trackball", false},
  };
  for (const Bind& b : binds) {
    if (!b.pane) {
      continue;
    }
    b.pane->set_view_host(b.host);
    if (host_->browser_->map_session()) {
      b.pane->set_map_contents(host_->browser_->map_session());
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
  host_->widget_.layout_contents();
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
  host_->for_each_map_viewport([](ui::views::MapViewport* pane) {
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
  if (host_->map_scene_ && host_->map_scene_->native_view() &&
      host_->map_scene_->attach_mode() != ui::views::MapViewport::AttachMode::kNone &&
      prefer_scene3d_stereo_gl()) {
    (void)host_->browser_->scene3d_stereo()->try_attach(host_->map_scene_->native_view());
  }
}


void MapPagesComposer::wire_map_scene() {
  auto paint2d_for = [this](ui::views::MapViewport* pane) {
    return [this, pane](HDC hdc, const RECT& rc) {
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      if (host_->browser_->blit()->in_preview() &&
          host_->browser_->blit()->present(hdc, w, h)) {
        return;
      }
      // GDI overlay paints into |hdc| (backbuffer DIB). FlyCube present_gpu
      // writes the DXGI swapchain ï¿?last_gpu_present_ok must NOT skip full
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
        host_->browser_->map2d()->paint_annotation_overlay(hdc, w, h);
      } else {
        host_->browser_->map2d()->paint(hdc, w, h, true);
        host_->browser_->blit()->capture(hdc, w, h);
      }
      content::ViewHost* host = host_->active_view_host();
      if (host_->browser_->flash_lit() && seh_view_host_flashing(host)) {
        host_->browser_->map2d()->paint_flash_overlay(hdc, w, h);
      }
    };
  };
  auto paint3d = [this](HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
      return;
    }
    const auto mode = host_->map_scene_ ? host_->map_scene_->attach_mode()
                                 : ui::views::MapViewport::AttachMode::kNone;
    // Priority: FlyCube ï¿?(opt-in) leftover GL stereo ï¿?ContentMapView ï¿?GDI DEM.
    const bool flycube = mode == ui::views::MapViewport::AttachMode::kFlyCube;
    const bool content_map =
        mode == ui::views::MapViewport::AttachMode::kContentMapView;
    const bool gpu_ok =
        flycube && host_->map_scene_ && host_->map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      host_->browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    // When FlyCube is the product SoT (default), never re-attach leftover GL
    // on the same HWND ï¿?try_present_sot would race the DX12 swapchain and
    // permanently stamp the HUD badge as Stereo/GL even after RHI recovers.
    // Stereo/GL is only allowed when the user selected that engine.
    const bool allow_stereo_fallback =
        prefer_scene3d_stereo_gl() && !flycube;
    HWND hwnd = host_->map_scene_ ? host_->map_scene_->native_view() : nullptr;
    if (allow_stereo_fallback &&
        host_->browser_->scene3d_stereo()->try_present_sot(
            hwnd, hdc, w, h, host_->browser_->orbit_frame()->yaw(),
            host_->browser_->orbit_frame()->pitch(),
            host_->browser_->orbit_frame()->distance())) {
      host_->browser_->scene3d()->set_render_engine_name("Stereo/GL");
      host_->browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    if (content_map) {
      // MapViewport already blitted ContentMapView when possible; HUD only.
      host_->browser_->scene3d()->set_render_engine_name("ContentMapView");
      host_->browser_->scene3d()->paint_hud(hdc, w, h);
      return;
    }
    // FlyCube attached but present not yet ok (or failed): software DEM SoT
    // into the paint DC. Keep the RHI label so the badge is not "Stereo/GL".
    if (flycube) {
      host_->browser_->scene3d()->set_render_engine_name("FlyCube/DX12");
    }
    host_->browser_->scene3d()->paint(hdc, w, h, /*fill_background=*/true);
  };
  for (ui::views::MapViewport* pane : {host_->map_edit_, host_->map_data_}) {
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
          return host_->browser_->map2d()->present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  if (host_->map_scene_) {
    host_->map_scene_->set_overlay_paint(paint3d);
    host_->map_scene_->set_gpu_present(
        [this](void* device, uint32_t w, uint32_t h) -> bool {
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr = nullptr;
          if (host_->map_scene_->snapshot_shell_overlay(&shell_copy, &shell, &gen)) {
            shell_ptr = &shell;
          }
          return host_->browser_->scene3d()->present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  // Seed shell overlay Commit for FlyCube / PresentMailbox (generation skip).
  host_->commit_widget_shell_to_maps();
  host_->wire_tool_seams();
  // Do not sync inspectors here: ResultPlaybackPanel scrubber paint during
  // init_shell AVd on skewed/stale panel* (heap corruption). init_shell and
  // session/plugin paths call sync_inspectors_from_scene after chrome is up.
}


void MapPagesComposer::commit_widget_shell_to_maps() {
  // Full-client seed (tab switch / overlay invalidate).
  host_->commit_widget_shell_to_maps(ui::views::Rect{});
}


void MapPagesComposer::commit_widget_shell_to_maps(const ui::views::Rect& dirty) {
  const ui::gfx::ShellRaster shell = host_->widget_.shell_raster();
  const std::uint64_t gen = host_->widget_.shell_generation();
  if (!shell.bgra || shell.width_px == 0 || shell.height_px == 0) {
    return;
  }
  // U3: unchanged published generation ï¿?skip crop/memcpy (gen skip).
  // Empty dirty is a full-seed (tab switch); still skip when gen matches.
  if (gen != 0 && gen == host_->last_shell_overlay_gen_) {
    return;
  }
  const uint32_t shell_stride =
      shell.stride_bytes != 0 ? shell.stride_bytes : shell.width_px * 4u;
  HWND widget_hwnd = host_->widget_.hwnd();
  // Empty dirty ï¿?publish to every pane. Non-empty ï¿?skip panes the paint did
  // not touch so menu/button hover does not memcpy+wake map HWNDs.
  const bool filter = !dirty.is_empty();
  host_->for_each_map_viewport([&](ui::views::MapViewport* pane) {
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
  if (gen != 0) {
    host_->last_shell_overlay_gen_ = gen;
  }
}


void MapPagesComposer::sync_flash_timer() {
  HWND h = host_->hwnd();
  if (!h) {
    return;
  }
  constexpr UINT_PTR kFlash = 0x464C5348u;
  SetPropW(h, L"SmtFlashBrowser", reinterpret_cast<HANDLE>(host_));
  content::ViewHost* host = host_->active_view_host();
  const bool on = seh_view_host_flashing(host);
  KillTimer(h, kFlash);
  if (!on) {
    host_->browser_->set_flash_lit(true);
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


void MapPagesComposer::wire_tool_seams() {
  // Snapshot owned host pointers once. Do not iterate a temporary list that
  // re-reads session getters after map2d/scene3d bind ï¿?a skewed MapSession
  // layout can poison trailing unique_ptrs mid-init_shell.
  content::ViewHost* const hosts[3] = {
      host_->browser_->edit_host(), host_->browser_->data_host(), host_->browser_->scene_host()};

  auto resolve = [this](const tool::Draft& draft) -> content::FeatureId {
    content::ViewHost* host = host_->active_view_host();
    if (!host) {
      return {};
    }
    tool::Workspace* ws = host->workspace();
    tool::Interaction* cur = ws ? ws->stack().current() : nullptr;
    const char* tool_id = cur ? cur->id() : "";
    if (!tool_id || draft.points.empty()) {
      return {};
    }
    const int px = draft.points.front().x_px;
    const int py = draft.points.front().y_px;
    double map_x = 0;
    double map_y = 0;
    host_->browser_->view_frame()->view_to_map(px, py, &map_x, &map_y);
    const double scale =
        host_->browser_->view_frame()->scale() > 1e-9 ? host_->browser_->view_frame()->scale() : 1.0;
    const double tol_map = 12.0 / scale;
    if (std::strncmp(tool_id, "select.", 7) == 0) {
      const MapScene::Feature* hit =
          host_->browser_->document()->hit_test(map_x, map_y, tol_map);
      return hit ? hit->id : content::FeatureId{};
    }
    if (std::strcmp(tool_id, "edit.vertex") == 0) {
      return host_->browser_->document()->move_selected_vertex(map_x, map_y, tol_map);
    }
    return {};
  };
  auto nav = [this](std::string_view command_id) -> content::Extent2 {
    int w = 800;
    int h = 600;
    host_->active_view_size(&w, &h);
    if (command_id == "view.full" || command_id == "view3d.full") {
      host_->browser_->view_frame()->fit_extent(*host_->browser_->document(), w, h);
      host_->browser_->orbit_frame()->apply_world_extent(
          host_->browser_->document()->world_extent());
      host_->browser_->push_shared_extent();
      host_->invalidate_map_overlays();
    } else if (command_id == "view.refresh") {
      host_->browser_->view_frame()->apply_world_extent(
          host_->browser_->view_frame()->view_world_extent(w, h), w, h);
      host_->browser_->push_shared_extent();
      host_->invalidate_map_overlays();
    }
    return host_->browser_->view_frame()->view_world_extent(w, h);
  };
  auto on_draft = [this](const tool::Draft& draft) {
    host_->browser_->handle_draft(draft);
  };
  auto map_project = [this](int x_px, int y_px, double* map_x, double* map_y) {
    host_->browser_->view_frame()->view_to_map(x_px, y_px, map_x, map_y);
  };
  // Guard against skewed MapSession layouts from parallel out/Debug rebuilds:
  // edit_host_ can be 0xCDCDCDCD / 0xCDCDCD00 and ViewHost::workspace AVs.
  tool::DraftCallback draft_cb = on_draft;
  tool::Workspace::FeatureHit hit_cb = resolve;
  tool::Workspace::NavCommand nav_cb = nav;
  tool::Workspace::MapProject project_cb = map_project;
  WorkspaceBindFns bind_fns{
      &trampoline_set_draft, &trampoline_set_hit, &trampoline_set_nav,
      &trampoline_set_project, &draft_cb,         &hit_cb,
      &nav_cb,               &project_cb};
  for (content::ViewHost* host : hosts) {
    if (!host || ptr_addr_poison(reinterpret_cast<uintptr_t>(host)) ||
        !ptr_mem_readable(host, sizeof(void*))) {
      LOGGING(LOG_WARNING, "wire_tool_seams: skip invalid ViewHost %p", host);
      continue;
    }
    tool::Workspace* ws = seh_view_host_workspace(host);
    if (!ws || ptr_addr_poison(reinterpret_cast<uintptr_t>(ws)) ||
        !ptr_mem_readable(ws, sizeof(void*))) {
      LOGGING(LOG_WARNING, "wire_tool_seams: skip invalid Workspace %p (host=%p)",
              ws, host);
      continue;
    }
    if (!seh_bind_workspace(ws, &bind_fns)) {
      LOGGING(LOG_WARNING,
              "wire_tool_seams: Workspace bind AV host=%p ws=%p (skip)", host,
              ws);
    }
  }
}


void MapPagesComposer::for_each_map_viewport(
    const std::function<void(ui::views::MapViewport*)>& fn) const {
  if (!fn) {
    return;
  }
  for (ui::views::MapViewport* pane : {host_->map_edit_, host_->map_data_, host_->map_scene_}) {
    if (pane) {
      fn(pane);
    }
  }
}


void MapPagesComposer::invalidate_map_overlays() {
  // If the shell DIB is not published yet, schedule a paint so the next
  // OnShellPublished can crop overlays; otherwise maps stay on a clear color.
  const ui::gfx::ShellRaster shell = host_->widget_.shell_raster();
  if (!shell.bgra || shell.width_px == 0 || shell.height_px == 0) {
    host_->widget_.schedule_paint();
  }
  // Keep shell generation in sync when map panes redraw without a shell paint.
  host_->commit_widget_shell_to_maps();
  host_->for_each_map_viewport([](ui::views::MapViewport* pane) {
    pane->invalidate_native();
  });
}


void MapPagesComposer::attach_hwnd_gestures() {
  // Showcase / self-test set SMT_SKIP_AMBOX_CATALOG and/or force ContentMapView.
  // HWND gesture subclass has AVed under parallel ninja (std::function _Tidy on
  // 0xcdcdcdcd). BMP export does not need pinch/pan subclass.
  auto env_is_one = [](const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] == '1' && v[1] == '\0';
  };
  if (env_is_one("SMT_SKIP_AMBOX_CATALOG") ||
      env_is_one("SMT_FORCE_CONTENT_MAPVIEW_2D") ||
      env_is_one("SMT_FORCE_GDI_MAP_OVERLAY")) {
    return;
  }
  auto on_pinch = [this](int x, int y, double scale) {
    host_->browser_->handle_pinch(x, y, scale);
  };
  auto on_pan = [this](int dx, int dy) {
    host_->browser_->handle_gesture_pan(dx, dy);
  };
  // Only wire gestures for panes that already own a present device. Data/3D
  // are HWND-only until first tab focus (see attach_viewports / switch_map_tab).
  // Prefer input_hwnd() (FlyCube DXGI popup when visible) ï¿?subclassing the
  // embed alone leaves pan/pinch/right-click dead under the present surface.
  auto try_attach = [&](ui::views::MapViewport* pane, MapHwndGestures* g) {
    if (!pane || !g) {
      return;
    }
    HWND hwnd = pane->input_hwnd();
    if (!hwnd || !IsWindow(hwnd)) {
      return;
    }
    if (pane->attach_mode() == ui::views::MapViewport::AttachMode::kNone) {
      return;
    }
    g->attach(hwnd, on_pinch, on_pan);
    host_->configure_gestures(g);
  };
  try_attach(host_->map_edit_, host_->browser_->edit_gestures());
  try_attach(host_->map_data_, host_->browser_->data_gestures());
  try_attach(host_->map_scene_, host_->browser_->scene_gestures());
}


void MapPagesComposer::configure_gestures(content::MapHwndGestures* gestures) {
  if (!gestures) {
    return;
  }
  gestures->set_right_click([this](HWND map_hwnd, int x, int y) {
    host_->on_map_right_click(map_hwnd, x, y);
  });
  gestures->set_extent_watch(
      [this](bool begin) { host_->browser_->on_extent_watch(begin); });
  gestures->set_viewport_resized([this]() { host_->browser_->refresh_scale(); });
}


void MapPagesComposer::active_view_size(int* w, int* h) const {
  int width = 800;
  int height = 600;
  if (ui::views::MapViewport* pane = host_->active_map()) {
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


void MapPagesComposer::switch_map_tab(int i) {
  content::push_record_event(
      "select_map_tab", std::string("{\"index\":") + std::to_string(i) + "}");
  if (host_->map_tabs_) {
    host_->map_tabs_->set_active(i);
    host_->map_tabs_->layout();
  }
  // Tab body bounds must be current before FlyCube Init / ShowWindow ï¿?  // deferred Data/3D panes were realize_native'd hidden; a stale 1x1 client
  // makes DX12 attach "succeed" then present a blank swapchain.
  host_->widget_.layout_contents();
  if (host_->map_tabs_) {
    host_->map_tabs_->layout();
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
    // Owned DXGI present popups are top-level ï¿?hiding the embed alone leaves
    // Map-Edit's present covering Scene3d (navy clear / wrong SoT).
    pane->set_flycube_present_visible(show);
  };
  sync_hwnd(host_->map_edit_, i == 0);
  sync_hwnd(host_->map_data_, i == 1);
  sync_hwnd(host_->map_scene_, i == 2);

  // 3D tab: seed atmosphere / stereo policy BEFORE lazy FlyCube attach so
  // abandon_mesh cannot race the present timer started by attach() (heap AV
  // with mine/stormsurge overlay TIN under FlyCube-default sessions).
  if (i == 2 && host_->map_scene_ && host_->browser_) {
    host_->browser_->scene3d()->bind_contents(host_->browser_->map_session(),
                                       host_->map_scene_->view_id());
    if (HWND hwnd = host_->map_scene_->native_view()) {
      if (prefer_scene3d_stereo_gl()) {
        (void)host_->browser_->scene3d_stereo()->try_attach(hwnd);
      } else {
        // Never call release()/destroy_ under FlyCube/GDI ï¿?stale leftover GL
        // teardown remaps heap (same class as mine Scene3D tab AV).
        host_->browser_->scene3d_stereo()->abandon();
      }
    }
    // Shared with --atmosphere-showcase=full. Seed before tool activate ï¿?    // trackball can emit a draft that nudges yaw off China framing.
    const ChinaScene3dAtmoFlags atmo =
        apply_china_scene3d_atmosphere(*host_->browser_);
    if (host_->atmosphere_panel_) {
      host_->atmosphere_panel_->set_ocean_checked(atmo.ocean);
      host_->atmosphere_panel_->set_cloud_checked(atmo.cloud);
      host_->atmosphere_panel_->set_sky_checked(atmo.sky);
      host_->atmosphere_panel_->set_fog_checked(atmo.fog);
    }
    LOGGING(LOG_INFO,
            "rhi.switch_map_tab scene3d ocean=%d cloud=%d sky=%d fog=%d",
            atmo.ocean ? 1 : 0, atmo.cloud ? 1 : 0, atmo.sky ? 1 : 0,
            atmo.fog ? 1 : 0);
  }

  // Lazy FlyCube: Data/3D were HWND-only at startup.
  if (ui::views::MapViewport* pane = host_->active_map()) {
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
      // Gestures: wait for attach_hwnd_gestures() at the end of this function.
      // Wiring mid-lazy-attach AVd in MapHwndGestures::detach/_Tidy
      // (0xCDCDCDCD) under browse.3d before orbit/tool activate settled.
    }
  }

  // TabStrip show/hides native map HWNDs; never destroy/recreate on switch.
  if (ui::views::MapViewport* pane = host_->active_map()) {
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

  if (content::ViewHost* host = host_->active_view_host()) {
    if (i == 2) {
      host->activate("view3d.trackball");
    } else {
      host->activate("view.pan");
    }
  }

  // China orbit AFTER tool activate ï¿?activate("view3d.trackball") historically
  // left yaw~0.42 (blank/navy) while showcase keeps ~2.59.
  if (i == 2 && host_->map_scene_ && host_->browser_) {
    apply_china_scene3d_orbit(*host_->browser_);
    LOGGING(LOG_INFO,
            "rhi.switch_map_tab scene3d orbit yaw=%.2f pitch=%.2f dist=%.2f",
            host_->browser_->orbit_frame()->yaw(), host_->browser_->orbit_frame()->pitch(),
            host_->browser_->orbit_frame()->distance());
    // Re-push stormsurge water TIN for the current playback index after
    // atmosphere abandon_mesh / orbit reset (overlay buffers survive abandon).
    auto& session = host_->browser_->analysis_playback();
    if (session.stormsurge_ready()) {
      (void)host_->browser_->apply_analysis_frame(session.frame_index());
    }
  }

  // Tab switch changes native HWND visibility + client size. Force a shell
  // repaint so WS_CLIPCHILDREN does not leave a hollow chrome hole, and kick
  // only the active map's next frame (avoid UpdateWindow / full overlay
  // invalidate during lazy attach ï¿?that re-entered ContentMapView paint).
  host_->widget_.schedule_paint();
  if (HWND shell = host_->widget_.hwnd()) {
    if (IsWindow(shell)) {
      InvalidateRect(shell, nullptr, FALSE);
    }
  }
  if (ui::views::MapViewport* pane = host_->active_map()) {
    pane->sync_native_bounds();
    pane->invalidate_native();
  }
  // Tab switch may have revealed a deferred FlyCube present ï¿?rebind gestures
  // onto input_hwnd() (Data / 3D lazy attach path).
  host_->attach_hwnd_gestures();
  host_->sync_status();
}


ui::views::MapViewport* MapPagesComposer::active_map() const {
  const int i = host_->map_tabs_ ? host_->map_tabs_->active() : 0;
  if (i == 1) {
    return host_->map_data_;
  }
  if (i == 2) {
    return host_->map_scene_;
  }
  return host_->map_edit_;
}


content::ViewHost* MapPagesComposer::active_view_host() const {
  const int i = host_->map_tabs_ ? host_->map_tabs_->active() : 0;
  if (i == 1) {
    return host_->browser_->data_host();
  }
  if (i == 2) {
    return host_->browser_->scene_host();
  }
  return host_->browser_->edit_host();
}


}  // namespace app
