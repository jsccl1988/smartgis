// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/shell_bind.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/pump/pump.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

bool window_action(Browser& browser, const std::string& action, int w, int h) {
  Browser* b = &browser;
  HWND hwnd = b->hwnd();
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  if (action == "activate") {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    return true;
  }
  if (action == "resize") {
    ui::views::DrawHost* scene = b->scene_draw_host();
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
    if (content::Map2dPresenter* map2d = b->map2d()) {
      map2d->note_surface_reset();
      map2d->invalidate_frame_cache();
    }
    if (BrowserUiDelegate* ui = b->ui()) {
      ui->for_each_draw_host([](ui::views::DrawHost* pane) {
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
        pane->sync_native_bounds();
        if (HWND map = pane->native_view()) {
          if (IsWindow(map)) {
            RECT rc = {};
            GetClientRect(map, &rc);
            if (rc.right > 0 && rc.bottom > 0) {
              SendMessageW(map, WM_SIZE, SIZE_RESTORED,
                           MAKELPARAM(rc.right, rc.bottom));
            }
            InvalidateRect(map, nullptr, FALSE);
          }
        }
        pane->invalidate_native();
      });
      ui->invalidate_map_overlays();
      ui->schedule_overlay_full_redraw();
    }
    pump_messages(80);
    return true;
  }
  return true;
}

}  // namespace

void bind_shell(Browser& browser,
                content::CapabilityHost* out,
                const wchar_t* mark_leaf) {
  Browser* b = &browser;
  const wchar_t* leaf =
      mark_leaf && mark_leaf[0] ? mark_leaf : kUiShowcaseMarkLeaf;

  out->pump = [](int ms) {
    pump_messages(static_cast<DWORD>(ms > 0 ? ms : 0));
  };
  out->mark = [leaf](const std::string& token) {
    write_mark(leaf, token.c_str(), false);
  };
  out->select_map_tab = [b](int index) { b->select_map_tab(index); };
  out->catalog_tab = [b](int index) {
    if (ui::views::CatalogView* cat = b->catalog_view()) {
      if (ui::views::TabStrip* tabs = cat->source_tabs()) {
        tabs->set_active(index);
      }
    }
  };
  out->inspector_tab = [b](int index) {
    if (b->ui()) {
      b->ui()->activate_inspector_tab(index);
    }
  };
  out->shell_hwnd = [b]() -> void* {
    return reinterpret_cast<void*>(b->hwnd());
  };
  out->window = [b](const std::string& action, int w, int h) {
    return window_action(*b, action, w, h);
  };
  out->key = [b](unsigned vk) {
    HWND hwnd = b->hwnd();
    if (!vk || !hwnd || !IsWindow(hwnd)) {
      return false;
    }
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    PostMessageW(hwnd, WM_KEYUP, vk, 0);
    return true;
  };
  out->clear_marks = [leaf]() { clear_mark(leaf); };
  out->suppress_dialogs = [](bool on) {
    ui::views::Dialog::set_dialog_modals_suppressed_for_test(on);
    return true;
  };
}

}  // namespace detail
}  // namespace app
