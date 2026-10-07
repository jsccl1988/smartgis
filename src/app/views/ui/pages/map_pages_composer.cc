// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/pages/map_pages_composer.h"
#include "app/views/ui/browser_view.h"

#include "app/views/browser/browser.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

#include "app/views/ui/pages/detail/seh_workspace.h"
#include "base/process/switches.h"
#include "content/public/view_host.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {

// Map/Data/3D tab horizon: viewports, gestures, overlays, tool seams.
// Bodies live in map_pages_{viewport,scene_wire,shell_overlay,tool_seams,
// gestures,tab_switch}.cc — same class, multi-TU (living shell §shell/ui composers).

MapPagesComposer::MapPagesComposer(BrowserView* host) : host_(host) {}

void MapPagesComposer::sync_flash_timer() {
  HWND h = host_->hwnd();
  if (!h) {
    return;
  }
  constexpr UINT_PTR kFlash = 0x464C5348u;
  SetPropW(h, L"FlashBrowser", reinterpret_cast<HANDLE>(host_));
  content::ViewHost* host = host_->active_view_host();
  const bool on = base::switch_cstr("map2d-showcase")
                      ? false
                      : detail::seh_view_host_flashing(host);
  KillTimer(h, kFlash);
  if (!on) {
    host_->browser_->set_flash_lit(true);
    return;
  }
  SetTimer(h, kFlash, 400, [](HWND hwnd, UINT, UINT_PTR, DWORD) {
    auto* self = reinterpret_cast<BrowserView*>(
        GetPropW(hwnd, L"FlashBrowser"));
    if (!self) {
      return;
    }
    self->browser_->set_flash_lit(!self->browser_->flash_lit());
    self->invalidate_map_overlays();
  });
}


void MapPagesComposer::for_each_draw_host(
    const std::function<void(ui::views::DrawHost*)>& fn) const {
  if (!fn) {
    return;
  }
  for (ui::views::DrawHost* pane : {host_->map_edit_, host_->map_scene_}) {
    if (pane) {
      fn(pane);
    }
  }
}


void MapPagesComposer::active_view_size(int* w, int* h) const {
  int width = 800;
  int height = 600;
  if (ui::views::DrawHost* pane = host_->active_map()) {
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


ui::views::DrawHost* MapPagesComposer::active_map() const {
  const int i = host_->map_tabs_ ? host_->map_tabs_->active() : 0;
  if (i == 1) {
    return host_->map_scene_;
  }
  return host_->map_edit_;
}


content::ViewHost* MapPagesComposer::active_view_host() const {
  const int i = host_->map_tabs_ ? host_->map_tabs_->active() : 0;
  if (i == 1) {
    return host_->browser_->scene_host();
  }
  return host_->browser_->edit_host();
}


}  // namespace app