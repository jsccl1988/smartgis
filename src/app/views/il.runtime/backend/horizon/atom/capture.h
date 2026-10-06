// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

// Shell client sample for visual gates. PrintWindow first, then a window-DC
// blit only when that sample has no visible pixels. Optional map and HUD
// children are composited into the shell DIB. A FlyCube DXGI present surface
// (WS_EX_NOREDIRECTIONBITMAP) is copied with a client-sized CAPTUREBLT, never
// a desktop blit of the whole shell. Flat fills are rejected.
bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd = nullptr,
                          HWND hud_hwnd = nullptr);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_
