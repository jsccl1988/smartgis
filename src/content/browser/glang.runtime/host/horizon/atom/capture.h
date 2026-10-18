// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {

// Shell client sample for visual gates. PrintWindow first, then a window-DC
// blit only when that sample has no visible pixels. Optional map and HUD
// children are composited into the shell DIB. A FlyCube DXGI present surface
// (WS_EX_NOREDIRECTIONBITMAP) is copied via desktop SRCCOPY of the TOPMOST
// Present rect. Flat fills are rejected.
bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd = nullptr,
                          HWND hud_hwnd = nullptr);

// Stamp a MapIR export BMP (24/32bpp) into the shell BMP at |map|'s client
// placement. DXGI flip BitBlt is not SoT (class-black or scanline smear).
// Returns true when the shell file was rewritten with carto pixels.
bool stamp_map_export_into_shell_bmp(const wchar_t* shell_bmp,
                                     HWND shell,
                                     HWND map,
                                     const wchar_t* map_export_bmp);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_CAPTURE_H_
