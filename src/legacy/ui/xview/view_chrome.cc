// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/xview/view_chrome.h"

#include "content/public/map_types.h"
#include "content/public/view_host.h"
#include "tool/camera_nav.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace ui {
namespace {

tool::PointerPinchTracker g_pinch;

bool dispatch_event(content::ViewHost* host, content::InputEvent e) {
  return host && host->dispatch_input(e);
}

bool handle_pointer(content::ViewHost* host, HWND hwnd, UINT message,
                    WPARAM wparam, LPARAM lparam) {
#if defined(WM_POINTERDOWN)
  const UINT id = GET_POINTERID_WPARAM(wparam);
  POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  if (hwnd) {
    ScreenToClient(hwnd, &pt);
  }
  if (message == WM_POINTERDOWN) {
    g_pinch.on_down(id, pt.x, pt.y);
    return g_pinch.contact_count() >= 2;
  }
  if (message == WM_POINTERUPDATE) {
    double scale = 0;
    if (g_pinch.on_move(id, pt.x, pt.y, &scale) && host) {
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = tool::scale_to_wheel_delta(scale);
      e.x_px = pt.x;
      e.y_px = pt.y;
      return host->dispatch_input(e);
    }
    return g_pinch.contact_count() >= 2;
  }
  if (message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED) {
    g_pinch.on_up(id);
    return true;
  }
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)lparam;
#endif
  return false;
}

}  // namespace

bool dispatch_chrome_message(content::ViewHost* host, UINT message,
                             WPARAM wparam, LPARAM lparam) {
  return dispatch_chrome_message(host, nullptr, message, wparam, lparam);
}

bool dispatch_chrome_message(content::ViewHost* host, HWND hwnd, UINT message,
                             WPARAM wparam, LPARAM lparam) {
  if (!host) {
    return false;
  }
#if defined(WM_POINTERDOWN)
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
      message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED) {
    return handle_pointer(host, hwnd, message, wparam, lparam);
  }
#endif
  content::InputEvent e{};
  switch (message) {
    case WM_MOUSEMOVE:
      e.kind = content::InputEvent::Kind::kMouseMove;
      break;
    case WM_LBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kLDown;
      break;
    case WM_LBUTTONUP:
      e.kind = content::InputEvent::Kind::kLUp;
      break;
    case WM_LBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kLDClick;
      break;
    case WM_RBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      break;
    case WM_RBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      break;
    case WM_RBUTTONDBLCLK:
      e.kind = content::InputEvent::Kind::kRDClick;
      break;
    case WM_MBUTTONDOWN:
      e.kind = content::InputEvent::Kind::kRDown;
      e.flags = MK_MBUTTON;
      break;
    case WM_MBUTTONUP:
      e.kind = content::InputEvent::Kind::kRUp;
      e.flags = MK_MBUTTON;
      break;
    case WM_MOUSEWHEEL: {
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam));
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
      if (e.flags & MK_SHIFT) {
        e.wheel = -e.wheel;
      }
      return host->dispatch_input(e);
    }
    case WM_KEYDOWN:
      e.kind = content::InputEvent::Kind::kKeyDown;
      e.key = static_cast<uint32_t>(wparam);
      return host->dispatch_input(e);
    default:
      return false;
  }
  e.x_px = static_cast<int32_t>(static_cast<short>(LOWORD(lparam)));
  e.y_px = static_cast<int32_t>(static_cast<short>(HIWORD(lparam)));
  if (e.flags == 0) {
    e.flags = static_cast<uint32_t>(wparam);
  }
  return dispatch_event(host, e);
}

}  // namespace ui
