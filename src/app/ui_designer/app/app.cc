// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/app/app.h"

#include <memory>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "app/ui_designer/shell/shell.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/widget/widget.h"

namespace app {
namespace {

DesignerShell* g_shell = nullptr;

VOID CALLBACK hot_reload_timer(HWND, UINT, UINT_PTR, DWORD) {
  if (g_shell) {
    g_shell->tick_hot_reload();
  }
}

VOID CALLBACK generate_hotkey_timer(HWND, UINT, UINT_PTR, DWORD) {
  if (g_shell) {
    g_shell->tick_generate_hotkey();
  }
}

}  // namespace

int run_ui_designer(const std::string& initial_path) {
  // Also invoked inside Widget::init; call early so Theme::current() is ready.
  ui::views::ThemeService::get().ensure_builtin_packs();
  ui::views::ThemeService::get().load_persisted();

  ui::views::Widget widget;
  ui::views::Widget::InitParams params;
  params.title = L"UiDesigner";
  params.width = 1280;
  params.height = 900;
  params.size_in_dips = true;
  params.frame_kind = ui::views::Widget::FrameKind::kCustom;
  if (!widget.init(params)) {
    return 1;
  }
  auto shell = std::make_unique<DesignerShell>();
  shell->build_ui();
  g_shell = shell.get();
  DesignerShell* raw = shell.get();
  auto frame = std::make_unique<ui::views::FrameView>();
  frame->set_title("UiDesigner");
  frame->set_can_maximize(true);
  frame->set_client(std::move(shell));
  widget.set_contents_view(std::move(frame));
  raw->open_path(initial_path.empty() ? "shell/main_app.ui.xml"
                                      : initial_path);
  SetTimer(widget.hwnd(), 1, 500, hot_reload_timer);
  SetTimer(widget.hwnd(), 2, 50, generate_hotkey_timer);
  widget.show();
  const int rc = widget.run_loop();
  KillTimer(widget.hwnd(), 1);
  KillTimer(widget.hwnd(), 2);
  g_shell = nullptr;
  return rc;
}

}  // namespace app
