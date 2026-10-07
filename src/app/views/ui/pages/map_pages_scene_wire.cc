// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "app/views/ui/pages/detail/seh_workspace.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_contents.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

void MapPagesComposer::wire_map_scene() {
  auto paint2d_for = [this](ui::views::DrawHost* pane) {
    return [this, pane](HDC hdc, const RECT& rc) {
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      // Product default 2D SoT = FlyCube present_gpu (DXGI). GDI overlay is
      // annotation/flash only once the present HWND is visible and carto drew.
      // ContentMapView / FORCE_GDI_MAP_OVERLAY remain harness opt-in fallbacks.
      const bool force_gdi = []() {
        if (const char* env = base::switch_cstr("force-gdi-map-overlay")) {
          return env[0] == '1' && env[1] == '\0';
        }
        return false;
      }();
      const bool content_map =
          pane &&
          pane->attach_mode() ==
              ui::views::DrawHost::AttachMode::kContentMapView;
      // StretchBlt pan/zoom preview is for FlyCube debounce. Under
      // ContentMapView + FORCE_GDI the preview DIB is often empty/cream.
      if (!force_gdi && !content_map &&
          host_->browser_->session().blit_in_preview() &&
          host_->browser_->session().blit_present(hdc, w, h)) {
        return;
      }
      content::BrowserSession& session = host_->browser_->session();
      // input_hwnd() returns the present popup only while it is visible �?      // skip full GDI only then, else the embed stays navy/ocean until reveal.
      const bool flycube_present_visible =
          pane &&
          pane->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent &&
          pane->input_hwnd() != nullptr &&
          pane->input_hwnd() != pane->native_view();
      const bool flycube_sot =
          flycube_present_visible && pane->last_gpu_present_ok() &&
          session.map2d_last_gpu_present_drew() &&
          session.map2d_layout_build_count() > 0;
      const bool scenic_2d = []() {
        const char* map_eng = base::switch_cstr("map2d-engine");
        return map_eng && map_eng[0] && _stricmp(map_eng, "scenic") == 0;
      }();
      if (!force_gdi && flycube_sot && !scenic_2d) {
        session.map2d_paint_annotation(hdc, w, h);
      } else {
        // Fallback: ContentMapView, GDI force, or GPU not yet revealed/drew.
        session.map2d_paint(hdc, w, h, true);
        session.blit_capture(hdc, w, h);
      }
      content::ToolSession* host = host_->active_tool_session();
      // Showcase first Widget::show paints before BrowserSession hosts are live;
      // flashing() then follows a dangling Workspace pimpl (cdb: tool_d
      // Workspace::flashing INVALID_POINTER_READ). Skip until after export.
      const bool showcase =
          base::switch_cstr("map2d-showcase") != nullptr;
      if (!showcase && host_->browser_->flash_lit() &&
          detail::seh_tool_session_flashing(host)) {
        host_->browser_->session().map2d_paint_flash(hdc, w, h);
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
                                 : ui::views::DrawHost::AttachMode::kNone;
    // Priority: FlyCube �?(opt-in) leftover GL stereo �?ContentMapView �?GDI DEM.
    const bool flycube = mode == ui::views::DrawHost::AttachMode::kGpuPresent;
    const bool content_map =
        mode == ui::views::DrawHost::AttachMode::kContentMapView;
    const bool gpu_ok =
        flycube && host_->map_scene_ && host_->map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      host_->browser_->session().scene3d_paint_hud(hdc, w, h);
      return;
    }
    // When FlyCube is the product SoT (default), never re-attach leftover GL
    // on the same HWND. try_present_sot would race the DX12 swapchain and
    // permanently stamp the HUD badge as Stereo/GL even after RHI recovers.
    // Stereo/GL is only allowed when the user selected that engine.
    const bool allow_stereo_fallback =
        content::BrowserSession::prefers_scene3d_stereo_gl() && !flycube;
    HWND hwnd = host_->map_scene_ ? host_->map_scene_->native_view() : nullptr;
    if (allow_stereo_fallback &&
        host_->browser_->session().try_present_scene3d_stereo(hwnd, hdc, w, h)) {
      host_->browser_->session().scene3d_set_render_engine_name("Stereo/GL");
      host_->browser_->session().scene3d_paint_hud(hdc, w, h);
      return;
    }
    if (content_map && host_->map_scene_ &&
        host_->map_scene_->last_content_present_ok()) {
      // SharedSurface actually blitted �?HUD only. Empty ContentMapView
      // (no GPU frames) must fall through to Scene3dPresenter::paint or the
      // 3D tab stays navy (scenic / GDI product HWND SoT).
      host_->browser_->session().scene3d_set_render_engine_name("ContentMapView");
      host_->browser_->session().scene3d_paint_hud(hdc, w, h);
      return;
    }
    // Product FlyCube: never bake soft DEM on the UI thread while waiting for
    // GpuPresent / first present_gpu. Soft paint holds present_mu_ and can
    // starve Display reveal (Engine:GDI Fps~0.02). Opaque + HUD only.
    if (content::BrowserSession::prefers_scene3d_flycube() && !gpu_ok) {
      host_->browser_->session().scene3d_set_render_engine_name("FlyCube/DX12");
      HBRUSH bg = CreateSolidBrush(RGB(120, 165, 210));
      RECT full = {0, 0, w, h};
      FillRect(hdc, &full, bg);
      DeleteObject(bg);
      host_->browser_->session().scene3d_paint_hud(hdc, w, h);
      if (host_->map_scene_) {
        host_->map_scene_->request_frame();
      }
      return;
    }
    // Explicit GDI / scenic / Content fallbacks: software DEM SoT.
    if (content::BrowserSession::prefers_scene3d_scenic()) {
      host_->browser_->session().scene3d_set_render_engine_name("scenic");
    } else if (content::BrowserSession::prefers_scene3d_gdi()) {
      host_->browser_->session().scene3d_set_render_engine_name("GDI");
    }
    // paint() already draws HUD under present_mu_; do not call paint_hud after
    // (nested lock was abort/exit 3 before present_mu_ became recursive).
    host_->browser_->session().scene3d_paint(hdc, w, h, /*fill_background=*/true);
  };
  for (ui::views::DrawHost* pane : {host_->map_edit_}) {
    if (!pane) {
      continue;
    }
    pane->set_overlay_paint(paint2d_for(pane));
    // Per-pane shell: shared Map2dPresenter content, pane-local DrawRequest.shell.
    pane->set_gpu_present(
        [this, pane](void* device, uint32_t w, uint32_t h) -> bool {
          // Display Init/Resize marks the swapchain dirty; consume here so
          // StaticReuse cannot keep a hollow backbuffer (flag was unused).
          if (pane->consume_gpu_surface_dirty()) {
            host_->browser_->session().note_map2d_surface_reset();
          }
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr = nullptr;
          if (pane->snapshot_shell_overlay(&shell_copy, &shell, &gen)) {
            shell_ptr = &shell;
          }
          return host_->browser_->session().map2d_present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  if (host_->map_scene_) {
    host_->map_scene_->set_overlay_paint(paint3d);
    host_->map_scene_->set_gpu_present(
        [this](void* device, uint32_t w, uint32_t h) -> bool {
          // Scene3d has no Map2d-style note_surface_reset; Display already
          // clears last_gpu_present_ok_ on Init/Resize. Do not consume the
          // dirty flag here (would drop it without forcing a redraw latch).
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr = nullptr;
          if (host_->map_scene_->snapshot_shell_overlay(&shell_copy, &shell, &gen)) {
            shell_ptr = &shell;
          }
          return host_->browser_->session().scene3d_present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
        });
  }
  // Seed shell overlay Commit for FlyCube / PresentMailbox (generation skip).
  host_->commit_widget_shell_to_maps();
  // Tool workspace binds run in BrowserView::finish_deferred_shell_wiring()
  // after WaitFirstMapPresent so WireShell / Browser.init stay off the gate.
  // Do not sync inspectors here: ResultPlaybackPanel scrubber paint during
  // init_shell AVd on skewed/stale panel* (heap corruption). init_shell and
  // session/plugin paths call sync_inspectors_from_scene after horizon is up.
}


}  // namespace app