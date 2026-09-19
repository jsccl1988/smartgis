// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/dialog_host.h"

#include "ui/views/kernel/dpi.h"

namespace ui {
namespace views {

void client_to_outer_size(int client_w_px,
                          int client_h_px,
                          DWORD style,
                          DWORD ex_style,
                          int* outer_w,
                          int* outer_h) {
  if (!outer_w || !outer_h) {
    return;
  }
  RECT rc = {0, 0, client_w_px > 0 ? client_w_px : 1,
             client_h_px > 0 ? client_h_px : 1};
  AdjustWindowRectEx(&rc, style, FALSE, ex_style);
  *outer_w = rc.right - rc.left;
  *outer_h = rc.bottom - rc.top;
  if (*outer_w < 1) {
    *outer_w = 1;
  }
  if (*outer_h < 1) {
    *outer_h = 1;
  }
}

OwnedPopupGeom center_outer_on_owner_rect(const RECT& owner_window_rect,
                                          int outer_w,
                                          int outer_h) {
  OwnedPopupGeom g;
  g.outer_width = outer_w > 0 ? outer_w : 1;
  g.outer_height = outer_h > 0 ? outer_h : 1;
  const int owner_w = owner_window_rect.right - owner_window_rect.left;
  const int owner_h = owner_window_rect.bottom - owner_window_rect.top;
  g.x = owner_window_rect.left + (owner_w - g.outer_width) / 2;
  g.y = owner_window_rect.top + (owner_h - g.outer_height) / 2;
  return g;
}

bool rect_approximately_centered(const RECT& popup,
                                 const RECT& owner,
                                 int tol_px) {
  if (tol_px < 0) {
    tol_px = 0;
  }
  const int popup_cx = (popup.left + popup.right) / 2;
  const int popup_cy = (popup.top + popup.bottom) / 2;
  const int owner_cx = (owner.left + owner.right) / 2;
  const int owner_cy = (owner.top + owner.bottom) / 2;
  const int dx = popup_cx > owner_cx ? popup_cx - owner_cx : owner_cx - popup_cx;
  const int dy = popup_cy > owner_cy ? popup_cy - owner_cy : owner_cy - popup_cy;
  return dx <= tol_px && dy <= tol_px;
}

OwnedPopupGeom place_owned_dialog(HWND owner,
                                  int client_w_dip,
                                  int client_h_dip,
                                  DWORD style,
                                  DWORD ex_style) {
  const float scale = scale_factor_from_dpi(dpi_for_hwnd(owner));
  int client_w = dip_to_px(client_w_dip, scale);
  int client_h = dip_to_px(client_h_dip, scale);
  if (client_w < 120) {
    client_w = 120;
  }
  if (client_h < 80) {
    client_h = 80;
  }
  int outer_w = 0;
  int outer_h = 0;
  client_to_outer_size(client_w, client_h, style, ex_style, &outer_w, &outer_h);

  OwnedPopupGeom g;
  g.outer_width = outer_w;
  g.outer_height = outer_h;
  if (owner && IsWindow(owner)) {
    RECT owner_rc = {};
    GetWindowRect(owner, &owner_rc);
    g = center_outer_on_owner_rect(owner_rc, outer_w, outer_h);
  } else {
    // No owner: park near the primary work-area origin (tests / headless).
    g.x = 40;
    g.y = 40;
  }
  clamp_rect_to_work_area(&g.x, &g.y, g.outer_width, g.outer_height, owner);
  return g;
}

}  // namespace views
}  // namespace ui
