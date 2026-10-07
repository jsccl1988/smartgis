// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/shell/shell_lifecycle_composer.h"

#include "app/views/ui/browser_view.h"
#include "app/views/ui/shell/detail/seh_seed.h"

#include <cstdint>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "app/views/browser/browser.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/session/browser_session.h"
#include "content/public/map_contents.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/primitives/menu/menu_bar.h"

namespace app {
namespace {
constexpr UINT_PTR kShellWheelSubclassId = 0x57484C45u;  // 'WHLE'
}  // namespace

ShellLifecycleComposer::ShellLifecycleComposer(BrowserView* host) : host_(host) {}

void ShellLifecycleComposer::install_shell_wheel_forward() {
  HWND shell = host_->widget_.hwnd();
  if (!shell || !IsWindow(shell) || host_->shell_wheel_subclassed_) {
    return;
  }
  if (SetWindowSubclass(shell, ShellLifecycleComposer::shell_wheel_subclass_proc, kShellWheelSubclassId,
                        reinterpret_cast<DWORD_PTR>(host_))) {
    host_->shell_wheel_subclassed_ = true;
  }
}


void ShellLifecycleComposer::remove_shell_wheel_forward() {
  HWND shell = host_->widget_.hwnd();
  if (host_->shell_wheel_subclassed_ && shell && IsWindow(shell)) {
    RemoveWindowSubclass(shell, ShellLifecycleComposer::shell_wheel_subclass_proc,
                         kShellWheelSubclassId);
  }
  host_->shell_wheel_subclassed_ = false;
}


LRESULT CALLBACK ShellLifecycleComposer::shell_wheel_subclass_proc(HWND hwnd, UINT msg,
                                                       WPARAM wparam,
                                                       LPARAM lparam,
                                                       UINT_PTR id,
                                                       DWORD_PTR data) {
  auto* self = reinterpret_cast<BrowserView*>(data);
  // Posted by deferred China seed when VIEWS_START_MAP_TAB is set — must
  // not nest select_map_tab inside the seed timer / switch_map_tab wait.
  constexpr UINT kReselectTab = WM_APP + 0x5354;  // 'ST'
  constexpr UINT kExtentChangedUi = WM_APP + 0x5253;  // 'RS'
  if (self && id == kShellWheelSubclassId && msg == kReselectTab) {
    const int idx = static_cast<int>(wparam);
    if (idx >= 0 && idx <= 2) {
      self->select_map_tab(idx);
      LOGGING(LOG_INFO, "startup: posted reselect map tab=%d after China seed",
              idx);
    }
    return 0;
  }
  if (self && id == kShellWheelSubclassId && msg == kExtentChangedUi) {
    auto* extent = reinterpret_cast<content::Extent2*>(lparam);
    if (extent) {
      if (self->browser_) {
        self->browser_->apply_extent_changed_on_ui(*extent);
      }
      delete extent;
    }
    return 0;
  }
  if (self && id == kShellWheelSubclassId &&
      (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL)) {
    // FlyCube present uses SW_SHOWNOACTIVATE; focus stays on horizon so wheel
    // arrives here. Forward when the cursor is over Map / Data / 3D input.
    const POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    for (ui::views::DrawHost* pane :
         {self->map_edit_, self->map_data_, self->map_scene_}) {
      if (!pane) {
        continue;
      }
      HWND map = pane->input_hwnd();
      if (!map || !IsWindow(map) || !IsWindowVisible(map)) {
        continue;
      }
      RECT rc = {};
      GetWindowRect(map, &rc);
      if (PtInRect(&rc, pt)) {
        SendMessageW(map, msg, wparam, lparam);
        return 0;
      }
    }
  }
  return DefSubclassProc(hwnd, msg, wparam, lparam);
}


void ShellLifecycleComposer::prepare_shell_close() {
  host_->remove_shell_wheel_forward();
  if (host_->map_edit_) {
    host_->map_edit_->detach();
  }
  if (host_->map_data_) {
    host_->map_data_->detach();
  }
  if (host_->map_scene_) {
    host_->map_scene_->detach();
  }
}


bool ShellLifecycleComposer::init_shell() {
  BASE_TRACE_EVENT("InitShell.body", "startup");
  {
    BASE_TRACE_EVENT("Widget.init", "startup");
    ui::views::Widget::InitParams params;
    params.title = L"SmartGIS Views";
    // Client DIPs (Widget scales + AdjustWindowRect). Physical-only 1280x800
    // looked ~853x533 on 150% DPI hosts.
    params.width = 1280;
    params.height = 800;
    params.size_in_dips = true;
    params.frame_kind = ui::views::Widget::FrameKind::kCustom;
    if (!host_->widget_.init(params)) {
      return false;
    }
  }
  // Mid snapshots → *.partial-*.txt; final dump is after first show only.
  base::trace::dump_startup_profile_partial("post-widget");
  host_->widget_.set_will_close([this]() {
    if (host_->browser_) {
      host_->browser_->prepare_close();
    }
  });
  host_->widget_.set_on_shell_published(
      [this](const ui::views::Rect& dirty) { host_->commit_widget_shell_to_maps(dirty); });
  host_->install_shell_wheel_forward();

  {
    BASE_TRACE_EVENT("BuildContents", "startup");
    host_->build_contents();
  }
  base::trace::dump_startup_profile_partial("post-build");
  {
    BASE_TRACE_EVENT("SeedDocument", "startup");
    // Showcase / harness set SKIP_AMBOX_CATALOG before Browser::init.
    // Skip china OGR bootstrap so plugin-showcase can reach Scene3D bodies;
    // product defer_china_seed() leaves the doc empty until Browser::show.
    // Real-data policy: never invent demo features on either path.
    const bool skip_china_seed = []() {
      const char* skip = base::switch_cstr("skip-ambox-catalog");
      return skip && skip[0] != '\0' && skip[0] != '0';
    }();
    const bool defer_china = host_->browser_->defer_china_seed();
    if (skip_china_seed || defer_china) {
      if (skip_china_seed) {
        std::fprintf(stderr,
                     "startup: SeedDocument empty (SKIP_AMBOX_CATALOG)\n");
      } else {
        std::fprintf(stderr,
                     "startup: SeedDocument empty (defer_china_seed)\n");
      }
      if (!detail::seh_seed_default(host_->browser_, /*allow_china=*/false)) {
        std::fprintf(stderr, "startup: SeedDocument clear SEH fail\n");
      }
    } else {
      // Match --ui-showcase=shell: open china_city before first paint. Skip
      // O(n×m) land-clip on this sync path so bare launch stays interactive;
      // hillshade still bakes on the first settled MapIR after show.
      base::set_switch("skip-china-land-clip", "1");
      {
        BASE_TRACE_EVENT("SeedDocument.Default", "startup");
        if (!detail::seh_seed_default(host_->browser_, /*allow_china=*/true)) {
          std::fprintf(stderr, "startup: SeedDocument china SEH fail\n");
        }
      }
      base::set_switch("skip-china-land-clip", "");
      std::fprintf(stderr, "startup: SeedDocument china=%d layers=%zu feats=%zu\n",
                   host_->browser_->session().document_has_china_extent() ? 1 : 0,
                   host_->browser_->session().document_layer_count(),
                   host_->browser_->session().document_feature_count());
    }
  }
  {
    BASE_TRACE_EVENT("BindPresenters", "startup");
    host_->browser_->session().bind_map_presenters();
    host_->browser_->pull_orbit_extent();
    // Fit world extent BEFORE FlyCube attach so the first display-thread
    // present_gpu uses a real camera (not a degenerate default extent).
    // Showcase skip-china seed: fit_map_extent AVd on demo-only document /
    // skewed ui_ hwnd during early init (cdb world3d-early). Scene3D framing
    // is applied later by apply_china_scene3d_product_defaults.
    const bool skip_fit = []() {
      const char* skip = base::switch_cstr("skip-ambox-catalog");
      return skip && skip[0] != '\0' && skip[0] != '0';
    }();
    if (!skip_fit) {
      if (!detail::seh_fit_and_push_extent(host_->browser_)) {
        std::fprintf(stderr, "startup: fit/push_shared_extent SEH fail\n");
      }
    }
    // Showcase SKIP_AMBOX_CATALOG: skip push_shared_extent too — under
    // parallel gis_d rebuilds it AVd after SeedDocument (exit 3, no marks).
    host_->wire_map_scene();
  }
  // Snapshot before FlyCube attach — often the slowest / hangiest startup step.
  base::trace::dump_startup_profile_partial("pre-attach");
  {
    BASE_TRACE_EVENT("AttachViewports", "startup");
    host_->attach_viewports();
  }
  host_->browser_->session().bind_scene3d_contents(
      host_->browser_->map_session(), host_->map_scene_ ? host_->map_scene_->view_id() : 0);
  host_->browser_->pull_orbit_extent();
  if (host_->browser_->map_session()) {
    host_->browser_->map_session()->SetObserver(host_->browser_);
  }
  {
    BASE_TRACE_EVENT("WireShell", "startup");
    // Re-fit after HWND sizes settle (layout may change client rect post-attach).
    // Same showcase skip as BindPresenters — demo-only seed AVs in fit_map_extent
    // / push_shared_extent (ui_ offset freefill under parallel ninja + SKIP_AMBOX).
    // ui.shell china seed later calls fit_map_extent (which pushes extent).
    const bool skip_fit_push = []() {
      const char* skip = base::switch_cstr("skip-ambox-catalog");
      return skip && skip[0] != '\0' && skip[0] != '0';
    }();
    if (!skip_fit_push) {
      if (!detail::seh_fit_and_push_extent(host_->browser_)) {
        std::fprintf(stderr, "startup: post-attach fit/push SEH fail\n");
      }
    }
    // Catalog, inspector sync, tool seams, and HWND gestures run in
    // finish_deferred_shell_wiring() after WaitFirstMapPresent (show_shell).
    // China 3D atmosphere (same defaults as --atmosphere-showcase=full) is
    // seeded on first switch to the 3D tab — see apply_china_scene3d_* in
    // switch_map_tab — so init_shell does not pay DEM/atmosphere cost before
    // the Map pane is interactive.
  }
  return true;
}


void ShellLifecycleComposer::finish_deferred_shell_wiring() {
  BASE_TRACE_EVENT("WireShell.deferred", "startup");
  host_->wire_catalog();
  host_->wire_edit_feedback();
  host_->wire_tool_seams();
  host_->sync_inspectors_from_scene();
  host_->sync_status();
  host_->attach_hwnd_gestures();
}


void ShellLifecycleComposer::show_shell() {
  BASE_TRACE_EVENT("ShowShell", "startup");
  {
    BASE_TRACE_EVENT("ShowWindow", "startup");
    host_->widget_.show();
  }
  // ShowWindow may present an empty compositor front (async raster). Re-layout
  // and schedule shell paint only — do not call host_->invalidate_map_overlays() here:
  // that syncs paint_map_content while ContentMapView / Map2dPresenter are
  // still settling and has AVd in Map2dSoftwarePainter (STL orphan) under
  // --self-test. Kick the active map HWND asynchronously (InvalidateRect,
  // no UpdateWindow).
  host_->widget_.layout_contents();
  // Catalog|Map splitter can lock a both-flex seed before preferred widths
  // settle (grey slab + squeezed map). Re-assert Catalog 288 DIP and reseed.
  if (host_->catalog_ && host_->catalog_map_) {
    host_->catalog_->set_preferred_size({288, 0});
    host_->catalog_map_->reseed();
    host_->widget_.layout_contents();
    LOGGING(LOG_INFO, "layout: catalog_map reseed catalog_w=%d map_tabs_x=%d",
            host_->catalog_->bounds().width,
            host_->map_tabs_ ? host_->map_tabs_->bounds().x : -1);
  }
  // Work splitter: re-pin inspector 320 and reseed so map_column stays flex
  // primary (SecondaryFixed) — proportional seed crushed Map beside Feature.
  if (host_->inspector_tabs_ && host_->catalog_map_) {
    if (ui::views::View* insp_host = host_->inspector_tabs_->parent()) {
      insp_host->set_preferred_size({320, 0});
    }
    host_->inspector_tabs_->set_preferred_size({320, 0});
    ui::views::View* map_col = host_->catalog_map_->parent();
    if (map_col) {
      map_col->set_preferred_size({0, 0});
    }
    if (map_col) {
      if (auto* work = dynamic_cast<ui::views::Splitter*>(map_col->parent())) {
        work->reseed();
        host_->widget_.layout_contents();
      }
    }
  }
  if (host_->menu_bar_) {
    // Re-measure File/Edit/View/Layer after DPI / font attach (DIP→px height).
    host_->menu_bar_->clear();
    host_->rebuild_menus();
    host_->widget_.layout_contents();
    host_->menu_bar_->schedule_paint();
  }
  // Re-seed main_split after the HWND client is final so Diagnostic Tools
  // preferred (DIP→px) is not locked against a create-time tiny inner height.
  if (host_->diagnostic_tools_) {
    for (ui::views::View* p = host_->diagnostic_tools_->parent(); p;
         p = p->parent()) {
      if (auto* split = dynamic_cast<ui::views::Splitter*>(p)) {
        split->reseed();
        host_->widget_.layout_contents();
        LOGGING(LOG_INFO,
                "layout: main_split reseed diagnostic_h=%d work_h=%d",
                host_->diagnostic_tools_->bounds().height,
                split->child_count() > 0 ? split->child_at(0)->bounds().height
                                         : -1);
        break;
      }
    }
  }
  host_->widget_.schedule_paint();
  if (HWND shell = host_->widget_.hwnd()) {
    if (IsWindow(shell)) {
      InvalidateRect(shell, nullptr, FALSE);
    }
  }
  // SKIP_AMBOX skips the 15s WaitFirstMapPresent; HWND record otherwise
  // captures empty compositor front (near-black horizon) + black map hole.
  host_->widget_.pump_until_shell_published(400);
  if (ui::views::DrawHost* pane = host_->active_map()) {
    pane->sync_native_bounds();
    // Init may have finished while the shell was still hidden; lift the DXGI
    // popup now that horizon is shown (inactive tabs stay hidden below).
    // Also bumps request_frame for the first china present. Do this after the
    // shell-publish pump so Display is not recording Pass during Resize from
    // first WM_PAINT (early kick made WaitFirstMapPresent worse).
    pane->set_gpu_present_visible(true);
    pane->invalidate_native();
    if (HWND map = pane->native_view()) {
      if (IsWindow(map)) {
        InvalidateRect(map, nullptr, FALSE);
      }
    }
    // Always drain a short first map paint so HWND capture is not a black
    // hole (SKIP_AMBOX still skips the 15s WaitFirstMapPresent below).
    {
      HWND shell_hwnd = host_->widget_.hwnd();
      const DWORD t_short = GetTickCount();
      while (shell_hwnd && IsWindow(shell_hwnd) &&
             GetTickCount() - t_short < 250u) {
        if (pane->last_content_present_ok() ||
            (host_->browser_->session().map2d_layout_build_count() > 0)) {
          break;
        }
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
        Sleep(10);
      }
    }
    // Product path: do not block shell interactivity on a full first map
    // present (FlyCube token + carto layout often ~2s+ and nested HillshadeBake).
    // Invalidate above already schedules the first frame; China seed (when
    // deferred) refreshes after show. Opt-in sync wait for harness / agents
    // that need a deterministic first carto frame before continuing:
    //   SYNC_FIRST_MAP_PRESENT=1
    // Showcase skips Map Edit present attach (SKIP_AMBOX_CATALOG) — never
    // spin waiting for a frame that will never arrive.
    const bool skip_wait = []() {
      const char* skip = base::switch_cstr("skip-ambox-catalog");
      if (skip && skip[0] != '\0' && skip[0] != '0') {
        return true;
      }
      const char* sync = base::switch_cstr("sync-first-map-present");
      const bool want_sync = sync && sync[0] == '1' && sync[1] == '\0';
      return !want_sync;
    }() || pane->attach_mode() == ui::views::DrawHost::AttachMode::kNone;
    if (!skip_wait) {
      BASE_TRACE_EVENT("WaitFirstMapPresent", "startup");
      uint32_t want = pane->frame_request();
      HWND shell_hwnd = host_->widget_.hwnd();
      const DWORD t0 = GetTickCount();
      const bool content_sot =
          pane->attach_mode() ==
          ui::views::DrawHost::AttachMode::kContentMapView;
      while (shell_hwnd && IsWindow(shell_hwnd) &&
             GetTickCount() - t0 < 15000u) {
        if (content_sot) {
          // GDI overlay / SharedSurface SoT: layout rebuild or content blit.
          if (pane->last_content_present_ok() ||
              (host_->browser_->session().map2d_layout_build_count() > 0)) {
            break;
          }
        } else {
          const bool token_ok = pane->last_gpu_present_ok() &&
                                pane->frame_presented() >= want;
          const bool drew_carto =
              host_->browser_->session().map2d_last_gpu_present_drew() &&
              host_->browser_->session().map2d_layout_build_count() > 0;
          if (token_ok && drew_carto) {
            break;
          }
          if (token_ok && !drew_carto) {
            // Skip (or empty layout) satisfied the token — force a real Pass
            // submit before leaving the pump.
            host_->browser_->session().note_map2d_surface_reset();
            pane->invalidate_native();
            want = pane->frame_request();
          }
        }
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
        }
        Sleep(10);
      }
    }
  }
  if (host_->map_data_ && host_->map_data_ != host_->active_map()) {
    host_->map_data_->set_gpu_present_visible(false);
  }
  if (host_->map_scene_ && host_->map_scene_ != host_->active_map()) {
    host_->map_scene_->set_gpu_present_visible(false);
  }
}


}  // namespace app
