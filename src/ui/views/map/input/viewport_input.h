// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_INPUT_H_
#define UI_VIEWS_MAP_VIEWPORT_INPUT_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
class ViewHost;
}

namespace ui {
namespace views {

class TouchMultitouchTracker;

namespace detail {

// Same Win32 → InputEvent mapping as leftover dispatch_shell_message for
// mouse / wheel / key.
bool route_view_host_input(content::ViewHost* host,
                           HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam,
                           bool suppress_mouse);

// Maps WM_POINTER* touch contacts into multitouch InputEvents.
bool route_view_host_pointer(content::ViewHost* host,
                             HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             TouchMultitouchTracker* tracker);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_INPUT_H_
