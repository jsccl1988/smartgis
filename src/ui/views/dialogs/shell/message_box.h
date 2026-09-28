// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_SHELL_MESSAGE_BOX_H_
#define UI_VIEWS_DIALOGS_SHELL_MESSAGE_BOX_H_

#include "ui/ui_views_export.h"
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

enum class MessageBoxKind { kInfo, kError };

// When true, show_message_box is a no-op (headless / unit tests).
UI_VIEWS_EXPORT void set_message_box_suppressed_for_test(bool suppressed);

UI_VIEWS_EXPORT void show_message_box(MessageBoxKind kind, const std::string& text);
// Prefer |owner| so the box is modal to the shell HWND and stays on-monitor.
UI_VIEWS_EXPORT void show_message_box(MessageBoxKind kind, const std::string& text, HWND owner);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_SHELL_MESSAGE_BOX_H_
