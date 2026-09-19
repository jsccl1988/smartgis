// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/dialogs/message_box.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {
namespace {

bool g_message_box_suppressed_for_test = false;

}  // namespace

void set_message_box_suppressed_for_test(bool suppressed) {
  g_message_box_suppressed_for_test = suppressed;
}

void show_message_box(MessageBoxKind kind, const std::string& text) {
  show_message_box(kind, text, nullptr);
}

void show_message_box(MessageBoxKind kind, const std::string& text, HWND owner) {
  if (g_message_box_suppressed_for_test) {
    return;
  }
  const std::wstring w = utf8_to_wide(text);
  const UINT type = (kind == MessageBoxKind::kError)
                        ? (MB_OK | MB_ICONERROR)
                        : (MB_OK | MB_ICONINFORMATION);
  MessageBoxW(owner && IsWindow(owner) ? owner : nullptr, w.c_str(), L"SmartGIS",
              type);
}

}  // namespace views
}  // namespace ui
