// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_PRESENT_HWND_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_PRESENT_HWND_H_

#include <cstdint>
#include <windows.h>

namespace app {
namespace detail {

// Options for a top-level FlyCube present surface (not a tiny tab child HWND).
struct ShowcasePresentHwndOpts {
  const wchar_t* class_name = nullptr;
  const wchar_t* window_title = nullptr;
  uint32_t width_px = 640;
  uint32_t height_px = 480;
};

// Registers (idempotent) |class_name| and creates a visible overlapped HWND.
HWND create_showcase_present_hwnd(const ShowcasePresentHwndOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_PRESENT_PRESENT_HWND_H_
