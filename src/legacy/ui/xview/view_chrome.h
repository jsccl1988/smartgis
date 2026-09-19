// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_XVIEW_VIEW_CHROME_H_
#define UI_XVIEW_VIEW_CHROME_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
class ViewHost;
}

namespace ui {

// Routes a Win32 pointer/key message into ViewHost. True if consumed.
// |hwnd| is required for WM_MOUSEWHEEL (screen → client) and WM_POINTER pinch.
bool dispatch_chrome_message(content::ViewHost* host, UINT message,
                             WPARAM wparam, LPARAM lparam);
bool dispatch_chrome_message(content::ViewHost* host, HWND hwnd, UINT message,
                             WPARAM wparam, LPARAM lparam);

}  // namespace ui

#endif  // UI_XVIEW_VIEW_CHROME_H_
