// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_WINDOW_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_WINDOW_H_

#include <cstdint>
#include <windows.h>

namespace app {
namespace detail {

// Top-level no-activate present window (harness / RHI), not a tab child HWND.
struct ShowcasePresentHwndOpts {
  const wchar_t* class_name = nullptr;
  const wchar_t* window_title = nullptr;
  uint32_t width_px = 640;
  uint32_t height_px = 480;
};

// Registers (idempotent) |class_name| and creates a visible overlapped HWND.
HWND create_showcase_present_hwnd(const ShowcasePresentHwndOpts& opts);

// DestroyWindow + null. No-op when |hwnd| is null or already null.
void destroy_showcase_present_hwnd(HWND* hwnd);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_WINDOW_H_
