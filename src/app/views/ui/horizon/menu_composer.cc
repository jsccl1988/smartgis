// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/menu_composer.h"

#include "app/views/ui/browser_view.h"

#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/browser.h"
#include "content/browser/session/browser_session.h"
#include "app/views/browser/commands/app_commands.h"
#include "app/views/browser/commands/view_commands.h"
#include "base/process/switches.h"
#include "content/public/view_host.h"
#include "ui/views/dialogs/select_one_dialog.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"

namespace app {
namespace {

void collect_bookmark_labels(Browser* browser,
                             std::vector<std::string>* labels) {
  if (!browser || !labels) {
    return;
  }
  labels->clear();
  const auto& bookmarks = browser->session().navigation_bookmarks();
  const size_t n = bookmarks.size();
  constexpr size_t kCap = 512;
  if (n == 0 || n > kCap) {
    return;
  }
  labels->reserve(n);
  for (const content::ViewBookmark& mark : bookmarks) {
    labels->push_back(mark.label);
  }
}


}  // namespace

MenuComposer::MenuComposer(BrowserView* host) : host_(host) {}

void MenuComposer::schedule_menu_rebuild() {
  HWND owner = host_->hwnd();
  if (!owner) {
    host_->rebuild_menus();
    return;
  }
  constexpr UINT_PTR kMenuTimer = 0x4D4E55u;
  SetPropW(owner, L"MenuBrowser", reinterpret_cast<HANDLE>(host_));
  KillTimer(owner, kMenuTimer);
  SetTimer(owner, kMenuTimer, 1,
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"MenuBrowser"));
             if (self) {
               self->rebuild_menus();
             }
           });
}


void MenuComposer::rebuild_menus() {
  if (!host_->menu_bar_ || !host_->browser_) {
    return;
  }
  std::vector<std::string> labels;
  collect_bookmark_labels(host_->browser_, &labels);
  ShellMenus menus = build_shell_menus(
      labels, [this](std::string_view id, int index) {
        if (id == "shell.open") {
          host_->browser_->on_open();
          return;
        }
        if (id == "shell.save") {
          host_->browser_->on_save_document();
          return;
        }
        if (id == "shell.export") {
          host_->browser_->on_export_document();
          return;
        }
        if (id == "shell.exit") {
          host_->on_exit();
          return;
        }
        if (id == "view.debug_console") {
          host_->toggle_debug_console();
          return;
        }
        if (id == "view.theme.dark") {
          ui::views::ThemeService::get().set_theme("dark");
          return;
        }
        if (id == "view.theme.light") {
          ui::views::ThemeService::get().set_theme("light");
          return;
        }
        if (id == "view.preferences") {
          std::vector<std::string> labels;
          std::vector<std::string> ids;
          for (const auto& pack : ui::views::ThemeService::get().packs()) {
            labels.push_back(pack.label);
            ids.push_back(pack.id);
          }
          std::string chosen;
          if (ui::views::SelectOneDialog::run(host_->widget_.hwnd(), labels,
                                              &chosen)) {
            for (size_t i = 0; i < labels.size(); ++i) {
              if (labels[i] == chosen) {
                ui::views::ThemeService::get().set_theme(ids[i]);
                break;
              }
            }
          }
          return;
        }
        if (id.starts_with("catalog.")) {
          host_->browser_->on_catalog_command(std::string(id));
          return;
        }
        host_->browser_->on_view_command(id, index, false, 0, 0);
      });
  host_->menu_bar_->clear();
  host_->menu_bar_->add_menu("File", std::move(menus.file));
  host_->menu_bar_->add_menu("Edit", std::move(menus.edit));
  host_->menu_bar_->add_menu("View", std::move(menus.view));
  host_->menu_bar_->add_menu("Layer", std::move(menus.layer));
}


void MenuComposer::on_map_right_click(HWND map_hwnd, int view_x, int view_y) {
  if (!map_hwnd || !host_->browser_) {
    return;
  }
  // TrackPopupMenu pumps messages; calling it from the map HWND subclass
  // during WM_RBUTTONUP re-enters the gesture/input stack and can AV. Defer
  // one tick like host_->schedule_menu_rebuild(MapLibre-like: RMB = menu only).
  host_->pending_map_menu_hwnd_ = map_hwnd;
  host_->pending_map_menu_x_ = view_x;
  host_->pending_map_menu_y_ = view_y;
  HWND owner = host_->hwnd();
  if (!owner) {
    host_->show_pending_map_context_menu();
    return;
  }
  constexpr UINT_PTR kMapCtxTimer = 0x4D4354u;  // 'MCT'
  SetPropW(owner, L"MapCtxBrowser", reinterpret_cast<HANDLE>(host_));
  KillTimer(owner, kMapCtxTimer);
  SetTimer(owner, kMapCtxTimer, 1,
           [](HWND timer_hwnd, UINT, UINT_PTR id, DWORD) {
             KillTimer(timer_hwnd, id);
             auto* self = reinterpret_cast<BrowserView*>(
                 GetPropW(timer_hwnd, L"MapCtxBrowser"));
             if (self) {
               self->show_pending_map_context_menu();
             }
           });
}


void MenuComposer::show_pending_map_context_menu() {
  HWND map_hwnd = host_->pending_map_menu_hwnd_;
  const int view_x = host_->pending_map_menu_x_;
  const int view_y = host_->pending_map_menu_y_;
  host_->pending_map_menu_hwnd_ = nullptr;
  if (!map_hwnd || !IsWindow(map_hwnd) || !host_->browser_) {
    return;
  }
  // Headless / self-test: skip modal popup (would hang the pump).
  if (base::switch_cstr("skip-map-context-menu")) {
    return;
  }
  std::vector<std::string> labels;
  collect_bookmark_labels(host_->browser_, &labels);
  const std::vector<ui::views::MenuItem> items = navigation_menu_items(
      labels, [this, view_x, view_y](std::string_view id, int index) {
        host_->browser_->on_view_command(id, index, true, view_x, view_y);
      });
  POINT pt{view_x, view_y};
  ClientToScreen(map_hwnd, &pt);
  ui::views::show_context_menu(map_hwnd, ui::views::Point{pt.x, pt.y}, items);
}


}  // namespace app
