// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHROME_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHROME_CAPTURE_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <windows.h>

namespace app {
namespace detail {

// BMP leaf under captures/ui/ for --ui-showcase=<mode>.
const wchar_t* ui_showcase_bmp_leaf(UiShowcaseMode mode);

// Capture shell HWND client for chrome visual gates. Uses window-DC blit only
// (never desktop DC). Rejects flat / non-diverse frames so blank scene
// PrintWindow fills are not written as bmp-ok.
bool capture_ui_chrome_bmp(HWND hwnd, const wchar_t* filename);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_CHROME_CAPTURE_H_
