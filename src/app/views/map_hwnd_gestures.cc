// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/map_hwnd_gestures.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <commctrl.h>
#include <windowsx.h>

namespace app {
namespace {

constexpr UINT_PTR kSubclassId = 0x53474D50u;  // 'SGMP'

}  // namespace

MapHwndGestures::~MapHwndGestures() {
  detach();
}

void MapHwndGestures::attach(HWND hwnd, PinchFn on_pinch) {
  detach();
  if (!hwnd || !IsWindow(hwnd) || !on_pinch) {
    return;
  }
  if (!SetWindowSubclass(hwnd, subclass_proc, kSubclassId,
                         reinterpret_cast<DWORD_PTR>(this))) {
    return;
  }
  hwnd_ = hwnd;
  on_pinch_ = std::move(on_pinch);
  // Want pinch zoom; block pan so two-finger pan reaches WM_POINTER* →
  // TouchMultitouchTracker instead of being swallowed as GID_PAN.
  GESTURECONFIG gc[2] = {};
  gc[0].dwID = GID_ZOOM;
  gc[0].dwWant = GC_ZOOM;
  gc[0].dwBlock = 0;
  gc[1].dwID = GID_PAN;
  gc[1].dwWant = 0;
  gc[1].dwBlock = GC_PAN;
  SetGestureConfig(hwnd_, 0, 2, gc, sizeof(GESTURECONFIG));
}

void MapHwndGestures::detach() {
  if (hwnd_ && IsWindow(hwnd_)) {
    RemoveWindowSubclass(hwnd_, subclass_proc, kSubclassId);
  }
  hwnd_ = nullptr;
  on_pinch_ = {};
  pinch_.reset();
  last_zoom_arg_ = 0;
  zooming_ = false;
}

LRESULT CALLBACK MapHwndGestures::subclass_proc(HWND hwnd, UINT msg,
                                                WPARAM wparam, LPARAM lparam,
                                                UINT_PTR id, DWORD_PTR data) {
  auto* self = reinterpret_cast<MapHwndGestures*>(data);
  if (self && id == kSubclassId) {
    if (msg == WM_NCDESTROY) {
      self->detach();
    } else if (self->on_message(msg, wparam, lparam)) {
      return 0;
    }
  }
  return DefSubclassProc(hwnd, msg, wparam, lparam);
}

bool MapHwndGestures::on_message(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_GESTURE) {
    return handle_gesture(lparam);
  }
#ifdef WM_POINTERDOWN
  if (msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE ||
      msg == WM_POINTERUP || msg == WM_POINTERLEAVE) {
    handle_pointer(msg, wparam, lparam);
    return false;
  }
#endif
  (void)wparam;
  return false;
}

bool MapHwndGestures::handle_gesture(LPARAM lparam) {
  GESTUREINFO gi = {};
  gi.cbSize = sizeof(gi);
  const auto handle = reinterpret_cast<HGESTUREINFO>(lparam);
  if (!GetGestureInfo(handle, &gi)) {
    return false;
  }
  // Only consume zoom. Closing other gesture handles after GetGestureInfo is
  // required; returning true prevents DefWindowProc from seeing a stale handle.
  if (gi.dwID != GID_ZOOM) {
    CloseGestureInfoHandle(handle);
    return true;
  }
  POINT pt = {gi.ptsLocation.x, gi.ptsLocation.y};
  if (hwnd_) {
    ScreenToClient(hwnd_, &pt);
  }
  if (gi.dwFlags & GF_BEGIN) {
    last_zoom_arg_ = gi.ullArguments;
    zooming_ = true;
  } else if (zooming_ && last_zoom_arg_ > 0 && gi.ullArguments > 0 &&
             on_pinch_) {
    const double scale = static_cast<double>(gi.ullArguments) /
                         static_cast<double>(last_zoom_arg_);
    last_zoom_arg_ = gi.ullArguments;
    if (scale > 0.0) {
      on_pinch_(pt.x, pt.y, scale);
    }
  }
  if (gi.dwFlags & GF_END) {
    zooming_ = false;
    last_zoom_arg_ = 0;
  }
  CloseGestureInfoHandle(handle);
  return true;
}

void MapHwndGestures::handle_pointer(UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef WM_POINTERDOWN
  const UINT32 id = GET_POINTERID_WPARAM(wparam);
  POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  if (hwnd_) {
    ScreenToClient(hwnd_, &pt);
  }
  if (msg == WM_POINTERDOWN) {
    pinch_.on_down(id, pt.x, pt.y);
    return;
  }
  if (msg == WM_POINTERUP || msg == WM_POINTERLEAVE) {
    pinch_.on_up(id);
    return;
  }
  if (msg == WM_POINTERUPDATE && on_pinch_) {
    double scale = 0;
    if (pinch_.on_move(id, pt.x, pt.y, &scale) && scale > 0.0) {
      // Deadband: pure two-finger pan must not micro-zoom.
      if (tool::scale_to_wheel_delta(scale) != 0) {
        on_pinch_(pt.x, pt.y, scale);
      }
    }
  }
#else
  (void)msg;
  (void)wparam;
  (void)lparam;
#endif
}

}  // namespace app
