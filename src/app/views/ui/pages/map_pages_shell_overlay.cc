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
#include <vector>

#include "ui/gfx/raster/shell_raster.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {

void MapPagesComposer::commit_widget_shell_to_maps() {
  // Full-client seed (tab switch / overlay invalidate).
  host_->commit_widget_shell_to_maps(ui::views::Rect{});
}


void MapPagesComposer::commit_widget_shell_to_maps(const ui::views::Rect& dirty) {
  // Copy under ShellCompositor::mu_ -- borrowed shell_raster() bits race the
  // raster worker's DIB swap/release and corrupt the process heap
  // (0xC0000374) when overlay crop memcpy runs unlocked.
  std::vector<std::uint8_t> shell_copy;
  ui::gfx::ShellRaster shell{};
  std::uint64_t gen = 0;
  if (!host_->widget_.copy_shell_raster(&shell_copy, &shell, &gen) ||
      !shell.bgra || shell.width_px == 0 || shell.height_px == 0) {
    return;
  }
  // Hidden tab bodies keep a full client rect after SW_HIDE -- they must not
  // receive BGRA crops (and must not keep per-pane "missing" true).
  auto pane_overlay_live = [](ui::views::DrawHost* pane) -> bool {
    if (!pane) {
      return false;
    }
    HWND hwnd = pane->native_view();
    if (!hwnd || !IsWindow(hwnd)) {
      // No HWND yet: still seed from View bounds (startup attach).
      return true;
    }
    return IsWindowVisible(hwnd) != FALSE;
  };
  // U3: unchanged published generation -- skip MapWindowPoints + BGRA memcpy.
  // Per-pane slots alone were wrong when they counted hidden map_scene_ as
  // missing: dirty-filtered edit publish + invalidate_map_overlays then
  // re-copied the scene HWND every time (overlay_copy_bytes 2.4).
  // Tab reveal clears last_shell_overlay_gen_ in switch_map_tab so HUD reseeds.
  if (gen != 0 && gen == host_->last_shell_overlay_gen_) {
    return;
  }
  const uint32_t shell_stride =
      shell.stride_bytes != 0 ? shell.stride_bytes : shell.width_px * 4u;
  HWND widget_hwnd = host_->widget_.hwnd();
  // Empty dirty: publish to every visible pane. Non-empty: skip panes the
  // paint did not touch so menu/button hover does not memcpy+wake map HWNDs.
  const bool filter = !dirty.is_empty();
  host_->for_each_draw_host([&](ui::views::DrawHost* pane) {
    if (!pane_overlay_live(pane)) {
      return;
    }
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

    BrowserView::LastShellOverlayCrop* slot = nullptr;
    BrowserView::LastShellOverlayCrop* empty = nullptr;
    for (auto& s : host_->last_shell_overlay_crops_) {
      if (s.pane == pane) {
        slot = &s;
        break;
      }
      if (!empty && s.pane == nullptr) {
        empty = &s;
      }
    }
    if (!slot) {
      // Never steal another pane's slot -- that would drop gen-skip for both.
      if (!empty) {
        return;
      }
      slot = empty;
    }

    // U3: skip BGRA memcpy when compositor gen is unchanged for this pane.
    // Same gen + size: origin jitter must not pass overlay_gen=0 (DrawHost
    // treats 0 as "always copy").
    const bool size_same = slot->pane == pane && slot->width == crop_w &&
                           slot->height == crop_h;
    if (gen != 0 && slot->gen == gen && size_same) {
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
    slot->pane = pane;
    slot->gen = gen;
    slot->x0 = x0;
    slot->y0 = y0;
    slot->width = crop_w;
    slot->height = crop_h;
  });
  // Mark gen consumed even when dirty-filter skipped every pane -- matches the
  // pre-regression U3 coalesce (tab switch clears this for HUD reseed).
  if (gen != 0) {
    host_->last_shell_overlay_gen_ = gen;
  }
}


void MapPagesComposer::invalidate_map_overlays() {
  // If the shell DIB is not published yet, schedule a paint so the next
  // OnShellPublished can crop overlays; otherwise maps stay on a clear color.
  // Do not borrow shell_raster() bits here -- generation alone is enough.
  if (host_->widget_.shell_generation() == 0) {
    host_->widget_.schedule_paint();
  }
  // Keep shell generation in sync when map panes redraw without a shell paint.
  host_->commit_widget_shell_to_maps();
  host_->for_each_draw_host([](ui::views::DrawHost* pane) {
    pane->invalidate_native();
  });
}


}  // namespace app
