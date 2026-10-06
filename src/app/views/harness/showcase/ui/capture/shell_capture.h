// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_
#define APP_VIEWS_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_

#include "app/views/app/cmdline/views_launch_options.h"

#include <windows.h>

namespace app {
namespace detail {

// BMP leaf under captures/ui/ for --ui-showcase=<mode>.
const wchar_t* ui_showcase_bmp_leaf(UiShowcaseMode mode);

// Capture shell HWND client for horizon visual gates. Window-DC / PrintWindow
// for horizon. When |map_hwnd| is a FlyCube DXGI present surface, composites
// that client via screen BitBlt (CAPTUREBLT) so WS_EX_NOREDIRECTIONBITMAP
// holes are not written as the map. Never desktop-blits the whole shell.
bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd = nullptr,
                          HWND hud_hwnd = nullptr);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_
