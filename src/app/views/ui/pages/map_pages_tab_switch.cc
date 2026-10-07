// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <string>

#include "app/views/browser/china_product_defaults.h"
#include "base/core/log.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/session/browser_session.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

void MapPagesComposer::switch_map_tab(int i) {
  if (i < 0) {
    return;
  }
  // Nested entry (shell publish pump Dispatching TabStrip input, or
  // plugin select_view_tab during lazy attach) must not start another
  // pump_until_shell_published — that hung plain no-arg launch.
  static thread_local bool in_switch = false;
  if (in_switch) {
    return;
  }
  struct SwitchGuard {
    bool& flag;
    explicit SwitchGuard(bool& f) : flag(f) { flag = true; }
    ~SwitchGuard() { flag = false; }
  } switch_guard(in_switch);
  if (host_->map_tabs_) {
    if (i >= host_->map_tabs_->tab_count()) {
      return;
    }
    if (host_->map_tabs_->active() == i) {
      // Same-tab reselect is normally a no-op. Recover when the 3D tab is
      // already selected but FlyCube never reached a live present (kNone /
      // software placeholder / GpuPresent with navy clear only).
      if (i == 1 && host_->map_scene_ &&
          content::BrowserSession::prefers_scene3d_flycube()) {
        const auto mode = host_->map_scene_->attach_mode();
        if (mode == ui::views::DrawHost::AttachMode::kGpuPresent) {
          if (!host_->map_scene_->last_gpu_present_ok()) {
            host_->map_scene_->set_gpu_present_visible(true);
            host_->map_scene_->resume_present_timer();
            host_->map_scene_->request_frame();
          }
          // Still enforce present z-order — do not return with Map chrome
          // while Scene3d is the live face (mouse set_active race).
        } else if (mode != ui::views::DrawHost::AttachMode::kNone) {
          host_->map_scene_->detach();
          // Fall through to lazy attach below.
        } else {
          // Fall through to lazy attach below.
        }
        if (mode == ui::views::DrawHost::AttachMode::kGpuPresent) {
          // Enforce visibility then return.
          auto sync_hwnd = [](ui::views::DrawHost* pane, bool show) {
            if (!pane) {
              return;
            }
            pane->set_gpu_present_visible(show);
          };
          sync_hwnd(host_->map_edit_, false);
          sync_hwnd(host_->map_scene_, true);
          host_->map_tabs_->schedule_paint();
          host_->widget_.schedule_paint();
          return;
        }
      } else {
        // Mouse can flip active_ before change_ while the other GPU present
        // stays visible — sync faces to the chrome index (visual_review #6).
        if (host_->map_edit_) {
          host_->map_edit_->set_gpu_present_visible(i == 0);
        }
        if (host_->map_scene_) {
          host_->map_scene_->set_gpu_present_visible(i == 1);
        }
        host_->map_tabs_->schedule_paint();
        host_->widget_.schedule_paint();
        return;
      }
    }
  }
  // Global ::content — draw_host.h also opens namespace content for
  // GisContents forward decls; keep the free helper unambiguous.
  ::  content::debug_agent().push_record_event(
      "select_view_tab", std::string("{\"index\":") + std::to_string(i) + "}");
  // Leaving 3D (interact Phase C): hide the Scene3d present popup BEFORE
  // layout_contents. Async-only hide races remasure and deadlocks UI↔Display
  // (rc 124, no interact-2d-b-ok). Match layout_gate: sync SW_HIDE on the
  // present HWND (no Display join), then KillTimer via pause_present.
  if (i != 1 && host_->map_scene_) {
    host_->map_scene_->set_gpu_present_visible(false);
    if (HWND present = host_->map_scene_->present_hwnd()) {
      if (IsWindow(present)) {
        ShowWindow(present, SW_HIDE);
      }
    }
    host_->map_scene_->pause_present();
  }
  // Entering 3D: stop Map Edit present + drain queued WM_TIMER before lazy
  // Scene attach. KillTimer-only / set_gpu_present_visible(false) leaves
  // present ticks that race the shared GPU process (self-test exit 3 after
  // data-ready under exe_smoke).
  if (i == 1 && host_->map_edit_) {
    host_->map_edit_->pause_present();
  }
  if (host_->map_tabs_) {
    host_->map_tabs_->set_active(i);
    host_->map_tabs_->layout();
    // Force TabStrip underline / header glyphs before the next shell paint
    // (plain-launch visual_review: Map stayed highlighted after env tab=3D).
    host_->map_tabs_->schedule_paint();
    host_->widget_.schedule_paint();
  }
  // Force overlay re-crop for the newly visible pane (published gen may be
  // unchanged across tab switch; U3 global skip would otherwise starve HUD).
  host_->last_shell_overlay_gen_ = 0;
  for (auto& s : host_->last_shell_overlay_crops_) {
    s.gen = 0;
  }
  // Tab body bounds must be current before FlyCube Init / ShowWindow �?  // deferred 3D pane was realize_native'd hidden; a stale 1x1 client
  // makes DX12 attach "succeed" then present a blank swapchain.
  host_->widget_.layout_contents();
  if (host_->map_tabs_) {
    host_->map_tabs_->layout();
  }

  // TabStrip show/hides native map HWNDs via View::set_visible; also force
  // Win32 visibility so self-test / rapid tab switches cannot leave the active
  // pane hidden when sync_native_bounds skips a no-op SetWindowPos.
  auto sync_hwnd = [](ui::views::DrawHost* pane, bool show) {
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
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                           SWP_ASYNCWINDOWPOS);
        }
      }
    }
    // Owned DXGI present popups are top-level �?hiding the embed alone leaves
    // Map-Edit's present covering Scene3d (navy clear / wrong SoT).
    pane->set_gpu_present_visible(show);
  };
  sync_hwnd(host_->map_edit_, i == 0);
  sync_hwnd(host_->map_scene_, i == 1);

  // 3D tab: seed atmosphere / stereo policy BEFORE lazy FlyCube attach so
  // abandon_mesh cannot race the present timer started by attach() (heap AV
  // with mine/stormsurge overlay TIN under FlyCube-default sessions).
  if (i == 1 && host_->map_scene_ && host_->browser_) {
    host_->browser_->session().bind_scene3d_document();
    host_->browser_->session().bind_scene3d_contents(
        host_->browser_->map_session(), host_->map_scene_->view_id());
    if (HWND hwnd = host_->map_scene_->native_view()) {
      if (content::BrowserSession::prefers_scene3d_stereo_gl()) {
        (void)host_->browser_->session().try_attach_scene3d_stereo(hwnd);
      } else {
        // Never call release()/destroy_ under FlyCube/GDI. Stale leftover GL
        // teardown remaps heap (same class as mine Scene3D tab AV).
        host_->browser_->session().abandon_scene3d_stereo();
      }
    }
    // Shared with --atmosphere-showcase=full. Seed before tool activate �?    // trackball can emit a draft that nudges yaw off China framing.
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

  // Lazy FlyCube: 3D was HWND-only at startup.
  if (ui::views::DrawHost* pane = host_->active_map()) {
    if (pane->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      LOGGING(LOG_INFO, "rhi.switch_map_tab lazy attach tab=%d", i);
      // Reseed Catalog|Map and main_split before Init �?a prior OS
      // window(resize) can leave PrimaryFixed catalog / SecondaryFixed
      // Diagnostic seeds crushing the deferred Scene3d HWND to ~40x93.
      bool restore_diagnostic = false;
      if (i == 1) {
        // Close Diagnostic before FlyCube Init. Interact Phase A2 (960×640) +
        // china catalog widen left Map/3D at ~316×101 and Init'd a stub
        // swapchain that later hangs export_bmp / Phase C. Restore after attach
        // so plain-launch 3D keeps the Output dock (visual_review #7).
        if (host_->diagnostic_tools_ &&
            host_->diagnostic_tools_->is_tools_visible()) {
          host_->diagnostic_tools_->set_visible_tools(false);
          restore_diagnostic = true;
        }
        // Re-pin Catalog to the product dock width before reseed �?china
        // catalog sync can leave preferred �?50 and crush the map column.
        if (host_->catalog_) {
          host_->catalog_->set_preferred_size({288, 0});
        }
        if (host_->catalog_map_) {
          host_->catalog_map_->reseed();
        }
        if (host_->diagnostic_tools_) {
          ui::views::View* diag_view = host_->diagnostic_tools_;
          for (ui::views::View* p = diag_view->parent(); p; p = p->parent()) {
            if (auto* split = dynamic_cast<ui::views::Splitter*>(p)) {
              split->reseed();
              break;
            }
          }
        }
        host_->widget_.layout_contents();
        if (host_->map_tabs_) {
          host_->map_tabs_->layout();
        }
        pane->sync_native_bounds();
      }
      // Size + show before Init so GetClientRect is the tab body, not 1x1.
      pane->sync_native_bounds();
      if (HWND hwnd = pane->native_view()) {
        if (IsWindow(hwnd)) {
          RECT rc = {};
          GetClientRect(hwnd, &rc);
          // Last resort: copy Map-Edit page bounds when Scene3d is still the
          // deferred stub (invisible sync skipped a real tab-body size).
          if ((rc.right < 640 || rc.bottom < 360) && host_->map_edit_) {
            const ui::views::Rect& eb = host_->map_edit_->bounds();
            if (eb.width >= 640 && eb.height >= 360) {
              pane->set_bounds(
                  {pane->bounds().x, pane->bounds().y, eb.width, eb.height});
              pane->sync_native_bounds();
              GetClientRect(hwnd, &rc);
            }
          }
          // Still crushed: take shell work area (catalog 288 + horizon).
          if ((rc.right < 640 || rc.bottom < 360) && host_->widget_.hwnd()) {
            RECT shell_rc = {};
            GetClientRect(host_->widget_.hwnd(), &shell_rc);
            const int want_w =
                (std::max)(640, static_cast<int>(shell_rc.right) - 360);
            const int want_h =
                (std::max)(400, static_cast<int>(shell_rc.bottom) - 160);
            if (want_w >= 640 && want_h >= 360) {
              pane->set_bounds(
                  {pane->bounds().x, pane->bounds().y, want_w, want_h});
              pane->sync_native_bounds();
              GetClientRect(hwnd, &rc);
            }
          }
          LOGGING(LOG_INFO,
                  "rhi.switch_map_tab pre-attach client=%dx%d bounds=%dx%d",
                  static_cast<int>(rc.right), static_cast<int>(rc.bottom),
                  pane->bounds().width, pane->bounds().height);
          ShowWindow(hwnd, SW_SHOW);
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
      }
      pane->attach();
      pane->sync_native_bounds();
      if (restore_diagnostic && host_->diagnostic_tools_) {
        host_->diagnostic_tools_->set_visible_tools(true);
        host_->widget_.layout_contents();
      }
      // Gestures: wait for attach_hwnd_gestures() at the end of this function.
      // Wiring mid-lazy-attach AVd in GisHwndGestures::detach/_Tidy
      // (0xCDCDCDCD) under browse.3d before orbit/tool activate settled.
    }
  }

  // TabStrip show/hides native map HWNDs; never destroy/recreate on switch.
  if (ui::views::DrawHost* pane = host_->active_map()) {
    if (HWND hwnd = pane->native_view()) {
      if (IsWindow(hwnd)) {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        // Post size notify �?SendMessage re-enters paint and has hung Phase C
        // (3D→Map) under china DEM / ContentMapView.
        if (rc.right > 0 && rc.bottom > 0) {
          PostMessageW(hwnd, WM_SIZE, SIZE_RESTORED,
                       MAKELPARAM(rc.right, rc.bottom));
        }
      }
    }
  }

  if (content::ToolSession* host = host_->active_tool_session()) {
    if (i == 1) {
      host->activate("view3d.trackball");
    } else {
      host->activate("view.pan");
    }
  }

  // China orbit AFTER tool activate �?activate("view3d.trackball") historically
  // left yaw~0.42 (blank/navy) while showcase keeps ~2.59.
  if (i == 1 && host_->map_scene_ && host_->browser_) {
    apply_china_scene3d_orbit(*host_->browser_);
    LOGGING(LOG_INFO,
            "rhi.switch_map_tab scene3d orbit yaw=%.2f pitch=%.2f dist=%.2f",
            host_->browser_->session().orbit_yaw(),
            host_->browser_->session().orbit_pitch(),
            host_->browser_->session().orbit_distance());
    // Re-present the current plugin playback frame after atmosphere
    // abandon_mesh / orbit reset (overlay buffers survive abandon).
    auto& session = host_->browser_->plugin_playback();
    if (session.frame_count() > 0) {
      (void)host_->browser_->apply_plugin_frame(session.frame_index());
    }
  }

  // Tab switch changes native HWND visibility + client size. Force a shell
  // repaint so WS_CLIPCHILDREN does not leave a hollow horizon hole, and kick
  // only the active map's next frame (avoid UpdateWindow / full overlay
  // invalidate during lazy attach — that re-entered ContentMapView paint).
  // Re-assert TabStrip active after reseed/layout: an in-flight shell
  // DisplayList can publish Map still underlined while Scene3d is live
  // (plain-launch visual_review #6).
  if (host_->map_tabs_) {
    host_->map_tabs_->set_active(i);
    host_->map_tabs_->schedule_paint();
  }
  host_->widget_.schedule_paint();
  if (HWND shell = host_->widget_.hwnd()) {
    if (IsWindow(shell)) {
      InvalidateRect(shell, nullptr, FALSE);
    }
  }
  host_->widget_.pump_until_shell_published(500);
  // Nested select_view_tab(0) during lazy attach / shell pump (plugin present,
  // catalog reseed) can SW_SHOW Map again. Re-pin chrome + HWND faces to |i|
  // so expect_scene_visible (harness rc 36/9) sees the right live face.
  if (host_->map_tabs_) {
    host_->map_tabs_->set_active(i);
  }
  if (i == 1 && host_->map_scene_) {
    HWND scene_hwnd = host_->map_scene_->native_view();
    if (!scene_hwnd || !IsWindow(scene_hwnd)) {
      LOGGING(LOG_WARNING,
              "rhi.switch_map_tab scene HWND dead after pump — reattach");
      host_->map_scene_->attach();
    }
  }
  sync_hwnd(host_->map_edit_, i == 0);
  sync_hwnd(host_->map_scene_, i == 1);
  if (ui::views::DrawHost* pane = host_->active_map()) {
    pane->sync_native_bounds();
    pane->invalidate_native();
    // Active pane must show its present surface. Using (i == 1) wrongly hid
    // Map-Edit GPU present on 3D→Map (Phase C) when FlyCube-2D was attached.
    pane->set_gpu_present_visible(true);
    pane->resume_present_timer();
  }
  // Rebind both tabs: 3D lazy attach needs input_hwnd(); 3D→Map must restore
  // Map-Edit subclass or pan/wheel after interact Phase C hits a dead HWND.
  host_->attach_hwnd_gestures();
  host_->sync_status();
  if (host_->map_tabs_) {
    LOGGING(LOG_INFO, "rhi.switch_map_tab chrome active=%d want=%d",
            host_->map_tabs_->active(), i);
  }
}


}  // namespace app