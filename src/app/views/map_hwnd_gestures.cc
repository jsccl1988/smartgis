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

void MapHwndGestures::attach(HWND hwnd, PinchFn on_pinch, PanFn on_pan) {
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
  on_pan_ = std::move(on_pan);
  // Pinch zoom + two-finger pan (left/right/any direction). Do not enable
  // single-finger pan flags — those would steal the mouse / one-finger path.
  GESTURECONFIG gc[2] = {};
  gc[0].dwID = GID_ZOOM;
  gc[0].dwWant = GC_ZOOM;
  gc[0].dwBlock = 0;
  gc[1].dwID = GID_PAN;
  gc[1].dwWant = GC_PAN | GC_PAN_WITH_INERTIA;
  gc[1].dwBlock = 0;
  SetGestureConfig(hwnd_, 0, 2, gc, sizeof(GESTURECONFIG));
}

void MapHwndGestures::detach() {
  if (hwnd_ && IsWindow(hwnd_)) {
    RemoveWindowSubclass(hwnd_, subclass_proc, kSubclassId);
  }
  hwnd_ = nullptr;
  on_pinch_ = {};
  on_pan_ = {};
  pinch_.reset();
  last_zoom_arg_ = 0;
  zooming_ = false;
  last_pan_x_ = 0;
  last_pan_y_ = 0;
  panning_ = false;
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
    // While GID_PAN owns the two-finger drag, swallow pointers so
    // TouchMultitouchTracker does not double-apply the same pan.
    if (panning_) {
      return true;
    }
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

  POINT pt = {gi.ptsLocation.x, gi.ptsLocation.y};
  if (hwnd_) {
    ScreenToClient(hwnd_, &pt);
  }

  if (gi.dwID == GID_PAN) {
    // Two-finger drag (including left/right) → map pan.
    if (gi.dwFlags & GF_BEGIN) {
      last_pan_x_ = pt.x;
      last_pan_y_ = pt.y;
      panning_ = true;
    } else if (panning_ && on_pan_) {
      const int dx = pt.x - last_pan_x_;
      const int dy = pt.y - last_pan_y_;
      last_pan_x_ = pt.x;
      last_pan_y_ = pt.y;
      if (dx != 0 || dy != 0) {
        on_pan_(dx, dy);
      }
    }
    if (gi.dwFlags & GF_END) {
      panning_ = false;
    }
    CloseGestureInfoHandle(handle);
    return true;
  }

  if (gi.dwID == GID_ZOOM) {
    if (gi.dwFlags & GF_BEGIN) {
      last_zoom_arg_ = gi.ullArguments;
      zooming_ = true;
    } else if (zooming_ && last_zoom_arg_ > 0 && gi.ullArguments > 0 &&
               on_pinch_) {
      const double scale = static_cast<double>(gi.ullArguments) /
                           static_cast<double>(last_zoom_arg_);
      last_zoom_arg_ = gi.ullArguments;
      if (scale > 0.0 && tool::scale_to_wheel_delta(scale) != 0) {
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

  CloseGestureInfoHandle(handle);
  return true;
}

void MapHwndGestures::handle_pointer(UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef WM_POINTERDOWN
  // During an active GID_PAN session, skip pinch sampling so pan is not
  // mixed with accidental scale from the same two contacts.
  if (panning_) {
    return;
  }
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
