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

#include "app/views/browser/nav/detail/seh_workspace.h"
#include "app/views/ui/pages/detail/viewport_hdc.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_contents.h"
#include "content/public/types.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/shell_raster.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/map/viewport/paint_policy.h"

namespace app {

void MapPagesComposer::wire_map_scene() {
  auto paint2d_for = [this](ui::views::DrawHost* pane) {
    return [this, pane](HDC hdc, const RECT& rc) {
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      // Product default 2D SoT = FlyCube present_gpu (DXGI). Selection/flash
      // strokes live on MapPass (MapIR overlay). Embed HDC is cold-fill only
      // while waiting for present HWND; full MapIR->GDI is harness opt-in.
      const bool force_gdi = ui::views::detail::force_gdi_map_overlay();
      const bool content_map =
          pane &&
          pane->attach_mode() ==
              ui::views::DrawHost::AttachMode::kContentMapView;
      const bool scenic_2d = ui::views::detail::prefer_map2d_scenic_engine();
      // StretchBlt pan/zoom preview is for FlyCube debounce. Under
      // ContentMapView + FORCE_GDI the preview DIB is often empty/cream.
      if (!force_gdi && !content_map &&
          host_->browser_->session().blit_in_preview() &&
          host_->browser_->session().blit_present(hdc, w, h)) {
        return;
      }
      content::BrowserSession& session = host_->browser_->session();
      const bool gpu_present_mode =
          pane &&
          pane->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent;
      const bool flycube_present_visible =
          gpu_present_mode && pane->input_hwnd() != nullptr &&
          pane->input_hwnd() != pane->native_view();
      // StaticReuse leaves last_gpu_present_drew() false even when the DXGI
      // swapchain still holds carto. Requiring drew here filled the embed with
      // ocean + request_frame forever and raced WaitFirstMapPresent resets.
      const bool flycube_sot =
          flycube_present_visible && pane->last_gpu_present_ok() &&
          session.map2d_layout_build_count() > 0;
      if (force_gdi || scenic_2d || content_map) {
        session.map2d_paint(hdc, w, h, true);
        session.blit_capture(hdc, w, h);
      } else if (flycube_sot) {
        // Selection/flash are on the DXGI present path -- nothing on embed HDC.
      } else if (gpu_present_mode) {
        detail::fill_hdc_and_request_frame(hdc, w, h, detail::kMap2dColdFillRgb,
                                           pane);
      } else {
        session.map2d_paint(hdc, w, h, true);
        session.blit_capture(hdc, w, h);
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
    // Priority: FlyCube -> (opt-in) leftover GL stereo -> ContentMapView.
    // No FORCE_GDI / soft DEM SoT -- HUD is vista HudPass on present_gpu.
    const bool flycube = mode == ui::views::DrawHost::AttachMode::kGpuPresent;
    const bool content_map =
        mode == ui::views::DrawHost::AttachMode::kContentMapView;
    const bool gpu_ok =
        flycube && host_->map_scene_ && host_->map_scene_->last_gpu_present_ok();
    if (gpu_ok) {
      // Swapchain already holds WorldPass + HudPass; HDC overlay is a no-op.
      return;
    }
    // When FlyCube is the product SoT (default), never re-attach leftover GL
    // on the same HWND. try_present_sot would race the DX12 swapchain.
    // Stereo/GL is only allowed when the user selected that engine.
    const bool allow_stereo_fallback =
        content::BrowserSession::prefers_scene3d_stereo_gl() && !flycube;
    HWND hwnd = host_->map_scene_ ? host_->map_scene_->native_view() : nullptr;
    if (allow_stereo_fallback &&
        host_->browser_->session().try_present_scene3d_stereo(hwnd, hdc, w, h)) {
      host_->browser_->session().scene3d_set_render_engine_name("Stereo/GL");
      return;
    }
    if (content_map && host_->map_scene_ &&
        host_->map_scene_->last_content_present_ok()) {
      host_->browser_->session().scene3d_set_render_engine_name(
          "ContentMapView");
      return;
    }
    // Product FlyCube waiting for first present_gpu: placeholder only.
    // Soft DEM removed -- fail closed and request another frame.
    if (content::BrowserSession::prefers_scene3d_flycube() && !gpu_ok) {
      host_->browser_->session().scene3d_set_render_engine_name("FlyCube/DX12");
      detail::fill_hdc_and_request_frame(hdc, w, h, detail::kScene3dColdFillRgb,
                                         host_->map_scene_);
      return;
    }
    // Scenic MemFrame (no soft DEM). GDI engine is placeholder only.
    if (content::BrowserSession::prefers_scene3d_scenic()) {
      host_->browser_->session().scene3d_set_render_engine_name("scenic");
      host_->browser_->session().scene3d_paint(hdc, w, h,
                                               /*fill_background=*/true);
      return;
    }
    if (content::BrowserSession::prefers_scene3d_gdi()) {
      host_->browser_->session().scene3d_set_render_engine_name("GDI");
      detail::fill_hdc_rgb(hdc, w, h, detail::kScene3dGdiPlaceholderRgb);
    }
  };
  for (ui::views::DrawHost* pane : {host_->map_edit_}) {
    if (!pane) {
      continue;
    }
    pane->set_overlay_paint(paint2d_for(pane));
    pane->set_eagle_eye_commit([this, pane](const content::Extent2& extent) {
      if (!host_->browser_) {
        return;
      }
      int w = 800;
      int h = 600;
      HWND surface = pane->present_hwnd();
      if (!surface || !IsWindow(surface)) {
        surface = pane->native_view();
      }
      if (surface && IsWindow(surface)) {
        RECT rc = {};
        GetClientRect(surface, &rc);
        if (rc.right > 32 && rc.bottom > 32) {
          w = rc.right;
          h = rc.bottom;
        }
      }
      content::BrowserSession& session = host_->browser_->session();
      session.apply_view_world_extent(extent, w, h);
      if (host_->browser_->navigation_baselined()) {
        session.navigation_commit(extent);
      }
      host_->browser_->push_shared_extent();
      host_->invalidate_map_overlays();
      host_->browser_->refresh_scale();
      pane->request_frame();
    });
    // Per-pane shell: shared Map2dPresenter content, pane-local DrawRequest.shell.
    pane->set_gpu_present(
        [this, pane](void* device, uint32_t w, uint32_t h) -> bool {
          // Display Init/Resize marks the swapchain dirty; consume here so
          // StaticReuse cannot keep a hollow backbuffer (flag was unused).
          if (pane->consume_gpu_surface_dirty()) {
            host_->browser_->session().note_map2d_surface_reset();
          }
          content::ToolSession* tool = host_->active_tool_session();
          const bool flash_pulse =
              host_->browser_->flash_lit() &&
              detail::seh_tool_session_flashing(tool);
          host_->browser_->session().map2d().set_flash_pulse(flash_pulse);
          std::vector<uint8_t> shell_copy;
          ui::gfx::ShellRaster shell{};
          uint64_t gen = 0;
          const ui::gfx::ShellRaster* shell_ptr =
              detail::borrow_pane_shell(pane, &shell_copy, &shell, &gen);
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
          const ui::gfx::ShellRaster* shell_ptr = detail::borrow_pane_shell(
              host_->map_scene_, &shell_copy, &shell, &gen);
          const bool ok = host_->browser_->session().scene3d_present_gpu(
              static_cast<render::rhi::Device*>(device), w, h, shell_ptr, gen);
          if (ok && host_->map_scene_) {
            // Align title / yellow identity bar with Engine HUD present FPS.
            host_->map_scene_->publish_present_fps(
                host_->browser_->session().scene3d().gpu().last_fps);
          }
          return ok;
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
