// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_DIALOG_HOST_H_
#define UI_VIEWS_KERNEL_DIALOG_HOST_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {

// Outer CreateWindow box for an owned popup / modal dialog.
struct OwnedPopupGeom {
  int x = 0;
  int y = 0;
  int outer_width = 0;
  int outer_height = 0;
};

// Default style for Views modal dialogs (matches Widget::init with owner).
inline constexpr DWORD kOwnedDialogStyle =
    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN;

// Client pixels → outer CreateWindow size for |style| / |ex_style|.
void client_to_outer_size(int client_w_px,
                          int client_h_px,
                          DWORD style,
                          DWORD ex_style,
                          int* outer_w,
                          int* outer_h);

// Center an outer box on |owner_window_rect| (screen coords). Pure geometry;
// does not query monitors.
OwnedPopupGeom center_outer_on_owner_rect(const RECT& owner_window_rect,
                                          int outer_w,
                                          int outer_h);

// True when |popup| center is within |tol_px| of |owner| center (screen).
bool rect_approximately_centered(const RECT& popup,
                                 const RECT& owner,
                                 int tol_px);

// Place a modal: DIP client size → DPI scale (from |owner|) → outer size →
// center on owner → clamp to work area. Safe when |owner| is null/invalid
// (uses screen DPI and CW-style origin near primary work area).
OwnedPopupGeom place_owned_dialog(HWND owner,
                                  int client_w_dip,
                                  int client_h_dip,
                                  DWORD style = kOwnedDialogStyle,
                                  DWORD ex_style = 0);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_DIALOG_HOST_H_
