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

// Routes a Win32 pointer/key/gesture message into ViewHost. True if consumed.
// |hwnd| is required for wheel (screen → client), WM_GESTURE pan/zoom, and
// WM_POINTER multitouch. Also handles WM_MOUSEHWHEEL (trackpad horizontal pan).
bool dispatch_chrome_message(content::ViewHost* host, UINT message,
                             WPARAM wparam, LPARAM lparam);
bool dispatch_chrome_message(content::ViewHost* host, HWND hwnd, UINT message,
                             WPARAM wparam, LPARAM lparam);

}  // namespace ui

#endif  // UI_XVIEW_VIEW_CHROME_H_
