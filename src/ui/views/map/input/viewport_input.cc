// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/input/viewport_input.h"

#include "ui/views/map/viewport/features.h"
#include "ui/views/map/input/touch_multitouch.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

namespace ui {
namespace views {
namespace detail {

// Same Win32 → InputEvent mapping as leftover dispatch_shell_message for
// mouse / wheel / key. Two-finger pan: WM_POINTER* → TouchMultitouchTracker
// (midpoint, pointer_count >= 2), matching CEF shell.js.
bool route_view_host_input(content::ViewHost* host,
                           HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam,
                           bool suppress_mouse) {
#ifdef SMT_HAS_VIEW_HOST
  if (!host) {
    return false;
  }
  if (suppress_mouse) {
    switch (message) {
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_LBUTTONDBLCLK:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_RBUTTONDBLCLK:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
        return true;  // swallow primary-contact mouse synthesis
      default:
        break;
    }
  }
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
    case WM_MOUSEHWHEEL: {
      // Trackpad / mouse horizontal wheel → pan (see input_flags::kHorizontalWheel).
      e.kind = content::InputEvent::Kind::kWheel;
      e.wheel = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wparam));
      e.flags = static_cast<uint32_t>(GET_KEYSTATE_WPARAM(wparam)) |
                content::input_flags::kHorizontalWheel;
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
      }
      e.x_px = pt.x;
      e.y_px = pt.y;
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
  e.flags = static_cast<uint32_t>(wparam);
  return host->dispatch_input(e);
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)lparam;
  (void)suppress_mouse;
  return false;
#endif
}

// Maps WM_POINTER* touch contacts into multitouch InputEvents.
bool route_view_host_pointer(content::ViewHost* host,
                             HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             TouchMultitouchTracker* tracker) {
#ifdef SMT_HAS_VIEW_HOST
  if (!host || !tracker || !hwnd) {
    return false;
  }
  switch (message) {
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERCAPTURECHANGED:
      break;
    default:
      return false;
  }

  const UINT32 pointer_id = GET_POINTERID_WPARAM(wparam);
  POINTER_INPUT_TYPE pointer_type = PT_POINTER;
  if (!GetPointerType(pointer_id, &pointer_type) ||
      pointer_type != PT_TOUCH) {
    return false;
  }

  POINTER_INFO info = {};
  if (!GetPointerInfo(pointer_id, &info)) {
    return false;
  }
  POINT pt = info.ptPixelLocation;
  ScreenToClient(hwnd, &pt);
  const int32_t x_px = pt.x;
  const int32_t y_px = pt.y;

  content::InputEvent e{};
  bool have_sample = false;
  if (message == WM_POINTERDOWN) {
    have_sample = tracker->on_contact_down(pointer_id, x_px, y_px, &e);
  } else if (message == WM_POINTERUPDATE) {
    have_sample = tracker->on_contact_move(pointer_id, x_px, y_px, &e);
  } else {
    // UP / CAPTURECHANGED: drop the contact (lost capture counts as lift).
    have_sample = tracker->on_contact_up(pointer_id, x_px, y_px, &e);
  }
  if (!have_sample) {
    // Multitouch session: swallow remaining pointer noise so DefWindowProc
    // does not synthesize mouse. Single-finger: fall through for mouse.
    return tracker->suppress_mouse();
  }
  return host->dispatch_input(e);
#else
  (void)host;
  (void)hwnd;
  (void)message;
  (void)wparam;
  (void)tracker;
  return false;
#endif
}

}  // namespace detail
}  // namespace views
}  // namespace ui
