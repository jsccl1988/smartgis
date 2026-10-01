// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_WIDGET_WIDGET_EVENT_H_
#define UI_VIEWS_KERNEL_WIDGET_WIDGET_EVENT_H_

#include "ui/ui_export.h"

#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Depth-first visible/enabled focusable views under |root|.
UI_EXPORT void collect_focusable_views(View* root, std::vector<View*>* out);

// Builds a MouseEvent from Win32 mouse message params (client or screen for
// wheel).
UI_EXPORT MouseEvent make_mouse_event(MouseEvent::Type type,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      int button,
                                      int wheel,
                                      HWND hwnd);

UI_EXPORT KeyEvent make_key_event(KeyEvent::Type type,
                                  WPARAM wparam,
                                  LPARAM lparam);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_WIDGET_WIDGET_EVENT_H_
