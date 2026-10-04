// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <windows.h>

namespace app {
namespace detail {

// BMP leaf under captures/ui/ for --ui-showcase=<mode>.
const wchar_t* ui_showcase_bmp_leaf(UiShowcaseMode mode);

// Capture shell HWND client for chrome visual gates. Uses window-DC blit only
// (never desktop DC). When |map_hwnd| is a live child map surface, composites
// that client into the shell DIB so PrintWindow holes are not false-green.
// Rejects flat / non-diverse frames so blank scene PrintWindow fills are not
// written as bmp-ok.
bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd = nullptr);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_CAPTURE_H_
