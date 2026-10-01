// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_WIDGET_WIDGET_LIFECYCLE_H_
#define UI_VIEWS_KERNEL_WIDGET_WIDGET_LIFECYCLE_H_

#include "ui/ui_export.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

// Top-level GetMessage loop until WM_QUIT. Returns PostQuitMessage code, or 1
// on GetMessage failure (never -1 / 0xFFFFFFFF).
UI_EXPORT int run_widget_quit_loop();

// Nested pump until |hwnd| is destroyed. Does not PostQuitMessage.
UI_EXPORT int run_widget_modal_loop(HWND hwnd);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_WIDGET_WIDGET_LIFECYCLE_H_
