// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/horizon/atom/surface.h"

#include <string_view>
#include <utility>

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

LPARAM client_lparam(int x, int y) {
  return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

HWND shell_hwnd(const Browser& browser) {
  HWND hwnd = browser.hwnd();
  if (!hwnd || !IsWindow(hwnd)) {
    return nullptr;
  }
  return hwnd;
}

bool post_mouse_pair(HWND hwnd, UINT down, UINT up, int x, int y) {
  if (!hwnd) {
    return false;
  }
  const LPARAM lp = client_lparam(x, y);
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, lp);
  PostMessageW(hwnd, down, (down == WM_RBUTTONDOWN) ? MK_RBUTTON : MK_LBUTTON,
               lp);
  PostMessageW(hwnd, up, 0, lp);
  return true;
}

// Left-button stroke: move to the first point, button down, move through the
// rest, button up. Shared by drag (2 points) and path (N points).
bool post_stroke(HWND hwnd, const int* xs, const int* ys, size_t count) {
  if (!hwnd || !xs || !ys || count < 2) {
    return false;
  }
  const LPARAM start = client_lparam(xs[0], ys[0]);
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, start);
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, start);
  for (size_t i = 1; i < count; ++i) {
    PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, client_lparam(xs[i], ys[i]));
  }
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(xs[count - 1], ys[count - 1]));
  return true;
}

void sync_draw_host_after_resize(ui::views::DrawHost* pane) {
  if (!pane) {
    return;
  }
  if (!pane->is_visible()) {
    if (pane->role() == ui::views::DrawHost::Role::kScene3d) {
      pane->pause_present();
    }
    pane->sync_native_bounds();
    return;
  }
  int prev_w = 0;
  int prev_h = 0;
  if (HWND before = pane->native_view()) {
    if (IsWindow(before)) {
      RECT prev = {};
      GetClientRect(before, &prev);
      prev_w = prev.right;
      prev_h = prev.bottom;
    }
  }
  pane->sync_native_bounds();
  if (HWND map = pane->native_view()) {
    if (IsWindow(map)) {
      RECT rc = {};
      GetClientRect(map, &rc);
      if (rc.right > 0 && rc.bottom > 0) {
        // Same client size: do not re-post WM_SIZE (would clear shell overlay
        // and rebuild DXGI before DrawHost same-size guard landed).
        if (rc.right != prev_w || rc.bottom != prev_h) {
          SendMessageW(map, WM_SIZE, SIZE_RESTORED,
                       MAKELPARAM(rc.right, rc.bottom));
          InvalidateRect(map, nullptr, FALSE);
          pane->invalidate_native();
        }
      }
    }
  }
}

}  // namespace

unsigned vk_from_name(const std::string& name) {
  static constexpr std::pair<std::string_view, unsigned> kAlias[] = {
      {"ESCAPE", VK_ESCAPE}, {"Esc", VK_ESCAPE},   {"RETURN", VK_RETURN},
      {"ENTER", VK_RETURN},  {"TAB", VK_TAB},      {"SPACE", VK_SPACE},
      {"CTRL", VK_CONTROL},  {"CONTROL", VK_CONTROL},
      {"SHIFT", VK_SHIFT},   {"ALT", VK_MENU},     {"MENU", VK_MENU},
  };
  for (const auto& [alias, vk] : kAlias) {
    if (name == alias) {
      return vk;
    }
  }
  if (name.size() == 1) {
    const char c = name[0];
    if (c >= 'a' && c <= 'z') {
      return static_cast<unsigned>(c - 'a' + 'A');
    }
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      return static_cast<unsigned>(c);
    }
  }
  return 0;
}

ShellSurface::ShellSurface(Browser& browser) : browser_(&browser) {}

bool ShellSurface::alive() const {
  return shell_hwnd(*browser_) != nullptr;
}

void* ShellSurface::hwnd_ptr() const {
  return reinterpret_cast<void*>(browser_->hwnd());
}

bool ShellSurface::click(int x, int y, int button, int clicks) const {
  HWND hwnd = shell_hwnd(*browser_);
  const UINT down = (button != 0) ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
  const UINT up = (button != 0) ? WM_RBUTTONUP : WM_LBUTTONUP;
  if (clicks >= 2) {
    if (!post_mouse_pair(hwnd, down, up, x, y)) {
      return false;
    }
    return post_mouse_pair(hwnd, WM_LBUTTONDBLCLK, WM_LBUTTONUP, x, y);
  }
  return post_mouse_pair(hwnd, down, up, x, y);
}

bool ShellSurface::drag(int x0, int y0, int x1, int y1) const {
  const int xs[] = {x0, x1};
  const int ys[] = {y0, y1};
  return post_stroke(shell_hwnd(*browser_), xs, ys, 2);
}

bool ShellSurface::wheel(int x, int y, int delta) const {
  HWND hwnd = shell_hwnd(*browser_);
  if (!hwnd) {
    return false;
  }
  POINT pt = {x, y};
  ClientToScreen(hwnd, &pt);
  PostMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<short>(delta)),
               MAKELPARAM(static_cast<WORD>(pt.x), static_cast<WORD>(pt.y)));
  return true;
}

bool ShellSurface::path(const std::vector<int>& xs,
                        const std::vector<int>& ys) const {
  if (xs.size() != ys.size()) {
    return false;
  }
  return post_stroke(shell_hwnd(*browser_), xs.data(), ys.data(), xs.size());
}

bool ShellSurface::key(unsigned vk, bool down) const {
  HWND hwnd = shell_hwnd(*browser_);
  if (!vk || !hwnd) {
    return false;
  }
  PostMessageW(hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, 0);
  return true;
}

bool ShellSurface::tap_key(unsigned vk) const {
  return key(vk, true) && key(vk, false);
}

bool ShellSurface::activate() const {
  HWND hwnd = shell_hwnd(*browser_);
  if (!hwnd) {
    return false;
  }
  ShowWindow(hwnd, SW_SHOW);
  SetForegroundWindow(hwnd);
  return true;
}

bool ShellSurface::resize(int w, int h) const {
  HWND hwnd = shell_hwnd(*browser_);
  if (!hwnd) {
    return false;
  }
  ui::views::DrawHost* scene = browser_->scene_draw_host();
  // Always pause Scene3d during shell resize — live FlyCube present on
  // Phase B2 has hung Display join. Do not auto-resume here; tab switch /
  // export_bmp / request_frame paths re-show present when needed.
  if (scene) {
    scene->pause_present();
  }
  const int nw = w > 0 ? w : 1280;
  const int nh = h > 0 ? h : 800;
  if (IsZoomed(hwnd) || IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }
  // Keep position; SWP_FRAMECHANGED so custom-frame WM_SIZE / layout
  // always run even when the outer size is unchanged.
  SetWindowPos(hwnd, nullptr, 0, 0, nw, nh,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  pump_messages(50);
  if (content::Map2dPresenter* map2d = browser_->map2d()) {
    map2d->note_surface_reset();
    map2d->invalidate_frame_cache();
  }
  if (BrowserUiDelegate* ui = browser_->ui()) {
    ui->for_each_draw_host(
        [](ui::views::DrawHost* pane) { sync_draw_host_after_resize(pane); });
    ui->invalidate_map_overlays();
    ui->schedule_overlay_full_redraw();
  }
  pump_messages(80);
  return true;
}

bool ShellSurface::action(const std::string& name, int w, int h) const {
  if (!shell_hwnd(*browser_)) {
    return false;
  }
  if (name == "activate") {
    return activate();
  }
  if (name == "resize") {
    return resize(w, h);
  }
  return true;
}

bool fill_hwnd_status(Browser& browser, content::HwndStatus* out) {
  if (!out) {
    return false;
  }
  *out = {};
  out->alive = ShellSurface(browser).alive() ? 1 : 0;
  return true;
}

}  // namespace detail
}  // namespace app
