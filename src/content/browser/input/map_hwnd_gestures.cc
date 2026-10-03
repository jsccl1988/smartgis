// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/input/map_hwnd_gestures.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <commctrl.h>
#include <windowsx.h>

namespace content {
namespace {

constexpr UINT_PTR kSubclassId = 0x53474D50u;  // 'SGMP'

}  // namespace

MapHwndGestures::~MapHwndGestures() {
  detach();
}

void MapHwndGestures::reset_state() {
  pinch_.reset();
  last_zoom_arg_ = 0;
  zooming_ = false;
  last_pan_x_ = 0;
  last_pan_y_ = 0;
  panning_ = false;
  extent_open_ = false;
  pinch_sampling_ = false;
  rdown_ = false;
}

void MapHwndGestures::configure_hwnd(HWND hwnd, PinchFn on_pinch,
                                     PanFn on_pan) {
  hwnd_ = hwnd;
  on_pinch_ = std::move(on_pinch);
  on_pan_ = std::move(on_pan);
  has_callbacks_ = true;
  reset_state();
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

void MapHwndGestures::clear_callbacks() {
  has_callbacks_ = false;
  // Assign empty when the slot looks live. Debug CRT poison in the first
  // pointer word (0xCDCDCDCDCDCDCDCD) means layout skew / UAF — _Tidy AVs
  // (browse.3d select_map_tab → attach → detach). Reconstruct in place.
  auto reset_fn = [](auto& fn) {
    using Fn = std::remove_reference_t<decltype(fn)>;
    uintptr_t word0 = 0;
    static_assert(sizeof(Fn) >= sizeof(uintptr_t), "std::function too small");
    std::memcpy(&word0, &fn, sizeof(word0));
    const bool poison = word0 == static_cast<uintptr_t>(0xCDCDCDCDCDCDCDCDULL) ||
                        word0 == static_cast<uintptr_t>(0xDDDDDDDDDDDDDDDDULL);
    if (poison) {
      std::memset(static_cast<void*>(&fn), 0, sizeof(Fn));
      std::construct_at(&fn);
    } else {
      fn = nullptr;
    }
  };
  reset_fn(on_pinch_);
  reset_fn(on_pan_);
  reset_fn(on_right_click_);
  reset_fn(on_extent_watch_);
  reset_fn(on_resized_);
}

void MapHwndGestures::set_right_click(RightClickFn fn) {
  on_right_click_ = std::move(fn);
}

void MapHwndGestures::set_extent_watch(ExtentWatchFn fn) {
  on_extent_watch_ = std::move(fn);
}

void MapHwndGestures::set_viewport_resized(ResizeFn fn) {
  on_resized_ = std::move(fn);
}

bool MapHwndGestures::begin_extent_sample() {
  if (extent_open_) {
    return false;
  }
  extent_open_ = true;
  if (on_extent_watch_) {
    on_extent_watch_(true);
  }
  return true;
}

void MapHwndGestures::end_extent_sample() {
  if (!extent_open_) {
    return;
  }
  extent_open_ = false;
  pinch_sampling_ = false;
  if (on_extent_watch_) {
    on_extent_watch_(false);
  }
}

void MapHwndGestures::attach(HWND hwnd, PinchFn on_pinch, PanFn on_pan) {
  // Skip detach when never wired — avoids tidy on unconstructed poison when
  // MapSession layout skew leaves has_callbacks_ non-zero garbage.
  if (hwnd_ || subclassed_ || has_callbacks_) {
    detach();
  }
  if (!hwnd || !IsWindow(hwnd) || !on_pinch) {
    return;
  }
  if (!SetWindowSubclass(hwnd, subclass_proc, kSubclassId,
                         reinterpret_cast<DWORD_PTR>(this))) {
    return;
  }
  subclassed_ = true;
  configure_hwnd(hwnd, std::move(on_pinch), std::move(on_pan));
}

void MapHwndGestures::bind(HWND hwnd, PinchFn on_pinch, PanFn on_pan) {
  if (hwnd_ || subclassed_ || has_callbacks_) {
    detach();
  }
  if (!hwnd || !IsWindow(hwnd) || !on_pinch) {
    return;
  }
  subclassed_ = false;
  configure_hwnd(hwnd, std::move(on_pinch), std::move(on_pan));
}

void MapHwndGestures::detach() {
  if (extent_open_) {
    end_extent_sample();
  }
  const bool had_subclass = subclassed_;
  const HWND hwnd = hwnd_;
  subclassed_ = false;
  hwnd_ = nullptr;
  if (had_subclass && hwnd && IsWindow(hwnd)) {
    RemoveWindowSubclass(hwnd, subclass_proc, kSubclassId);
  }
  clear_callbacks();
  reset_state();
}

bool MapHwndGestures::try_handle(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (!hwnd_) {
    return false;
  }
  return on_message(msg, wparam, lparam);
}

LRESULT CALLBACK MapHwndGestures::subclass_proc(HWND hwnd, UINT msg,
                                                WPARAM wparam, LPARAM lparam,
                                                UINT_PTR id, DWORD_PTR data) {
  auto* self = reinterpret_cast<MapHwndGestures*>(data);
  if (self && id == kSubclassId) {
    if (msg == WM_NCDESTROY) {
      self->detach();
    } else {
      bool wheel_sample = false;
      if (msg == WM_LBUTTONDOWN) {
        self->begin_extent_sample();
      } else if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        wheel_sample = self->begin_extent_sample();
      } else if (msg == WM_RBUTTONDOWN) {
        self->rdown_ = true;
        self->rdown_x_ = GET_X_LPARAM(lparam);
        self->rdown_y_ = GET_Y_LPARAM(lparam);
      }
      const bool consumed = self->on_message(msg, wparam, lparam);
      LRESULT result = 0;
      if (!consumed) {
        result = DefSubclassProc(hwnd, msg, wparam, lparam);
      }
      if (msg == WM_LBUTTONUP || wheel_sample) {
        self->end_extent_sample();
      }
      if (msg == WM_SIZE && self->on_resized_) {
        self->on_resized_();
      }
      if (msg == WM_RBUTTONUP && self->rdown_) {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        const int dx = x - self->rdown_x_;
        const int dy = y - self->rdown_y_;
        const int adx = dx < 0 ? -dx : dx;
        const int ady = dy < 0 ? -dy : dy;
        self->rdown_ = false;
        if (adx <= 4 && ady <= 4 && self->on_right_click_) {
          self->on_right_click_(hwnd, x, y);
        }
      }
#ifdef WM_POINTERUP
      if (msg == WM_POINTERUP && self->pinch_sampling_) {
        self->end_extent_sample();
      }
#endif
      if (consumed) {
        return 0;
      }
      return result;
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

  const bool nav_gesture = gi.dwID == GID_PAN || gi.dwID == GID_ZOOM;
  if (nav_gesture && (gi.dwFlags & GF_BEGIN)) {
    begin_extent_sample();
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
      end_extent_sample();
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
    if (nav_gesture && (gi.dwFlags & GF_END)) {
      end_extent_sample();
    }
    CloseGestureInfoHandle(handle);
    return true;
  }

  if (nav_gesture && (gi.dwFlags & GF_END)) {
    end_extent_sample();
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
        if (!pinch_sampling_) {
          pinch_sampling_ = begin_extent_sample();
        }
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

}  // namespace content
